// System Telemetry v1 HUD integration — trace geometry (timestamp placement,
// gaps, states, caching) and the renderer's update path against the real
// immutable HudFrameData::systemVitals snapshot produced by the real
// sysvitals::SystemVitals collector (fake source, injected time).
//
// OF-linked (HudWireframeRenderer): no window/GL context is created; only the
// update path runs. Draw is verified in the running app by the opt-in
// SystemVitalsHudHarness.

#include "../hud-compositor/HudVitalsTrace.h"
#include "../hud-compositor/HudVitalsDial.h"
#include "../hud-compositor/HudWireframeRenderer.h"
#include "../hud-compositor/HudProfileCompiler.h"
#include "../hud-compositor/HudRealFrameResolver.h"
#include "../hud-compositor/HudPresentationProfile.h"
#include "../hud-compositor/HudRegionCatalog.h"
#include "../hud-compositor/HudVocabularyResolver.h"
#include "../hud-runtime/HudFrameData.h"
#include "SystemVitals.h"
#include "CrtRegistries.h"
#include "HudInteractionStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <string>

using namespace hudpresent;
using sysvitals::State;

namespace {
int g_total = 0, g_failures = 0;
#define CHECK(cond, what) do { g_total++; if (!(cond)) { g_failures++; std::cerr << "FAIL: " << (what) << "  (" << __LINE__ << ")\n"; } } while (0)
bool near(float a, float b, float eps = 0.02f) { return std::fabs(a - b) <= eps; }
} // namespace

// Global allocation counter (to prove update() does not allocate).
static size_t g_allocs = 0;
void* operator new(size_t n) { g_allocs++; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

namespace {

// Builds a series (any capacity) from {t, value, valid} triples.
struct Pt { double t; float v; bool valid; };
template <size_t N>
sysvitals::SeriesSnapshot<N> makeSeries(std::initializer_list<Pt> pts, State st = State::Valid, uint64_t rev = 1) {
	sysvitals::SeriesSnapshot<N> s;
	s.state = st;
	s.revision = rev;
	for (const auto& p : pts) {
		s.history[s.count++] = sysvitals::Sample{p.t, p.v, p.valid};
		if (p.valid) { s.hasLatest = true; s.latest = p.v; s.latestTimeSeconds = p.t; }
	}
	if (st == State::Unavailable) s.hasLatest = false;
	return s;
}

const VitalsMetricSpec& specFps() { return vitalsSpecFor(sysvitals::Metric::Fps); }
const VitalsMetricSpec& specFt() { return vitalsSpecFor(sysvitals::Metric::FrameTimeMs); }
const VitalsMetricSpec& specCpu() { return vitalsSpecFor(sysvitals::Metric::CpuPercent); }
const VitalsMetricSpec& specGpu() { return vitalsSpecFor(sysvitals::Metric::GpuTempC); }

void testSpecs() {
	CHECK(vitalsSpecForSource("runtime.vitals.fps") == &specFps(), "fps spec by source id");
	CHECK(vitalsSpecForSource("runtime.vitals.frame_time_max") == &specFt(), "frame spec by source id");
	CHECK(vitalsSpecForSource("runtime.vitals.rss") != nullptr && vitalsSpecForSource("runtime.fps") == nullptr, "only vitals ids map");
	CHECK(specFps().windowSeconds == 60.0 && specFt().windowSeconds == 60.0 && specCpu().windowSeconds == 60.0, "fps/frame/cpu are 60 s");
	CHECK(specGpu().windowSeconds == 300.0 && vitalsSpecFor(sysvitals::Metric::RssMiB).windowSeconds == 300.0, "gpu/rss are 5 min");
	CHECK(near(specFt().referenceValue, 33.3f), "frame-time reference is 33.3 ms");
}

void testTimestampPlacement() {
	// 1 Hz cpu samples over 60 s: x follows the timestamp.
	sysvitals::SeriesSnapshot<60> s;
	s.state = State::Valid; s.hasLatest = true; s.revision = 1;
	for (int i = 0; i < 60; ++i) s.history[s.count++] = sysvitals::Sample{static_cast<double>(i), 10.0f, true};
	s.latest = 10.0f;
	HudVitalsTrace tr;
	tr.update(specCpu(), vitalsViewOf(s), 240.0f, 18.0f);
	CHECK(tr.vertexCount() == 60 && tr.segmentCount() == 1, "60 points, one segment");
	// newest (t=59) is the right edge; t=0 sits 59/60 of the way left.
	CHECK(near(tr.vertex(59).x, 240.0f), "newest at right edge");
	CHECK(near(tr.vertex(0).x, 240.0f * (1.0f / 60.0f)), "t=0 placed by timestamp (1/60 of plot)");
	CHECK(near(tr.vertex(30).x - tr.vertex(29).x, 4.0f), "uniform spacing = 4 px/s");

	// Irregular intervals: spacing is proportional to time, not index.
	auto irr = makeSeries<60>({{0.0, 10, true}, {1.0, 10, true}, {3.0, 10, true}, {3.5, 10, true}}, State::Valid, 2);
	tr.update(specCpu(), vitalsViewOf(irr), 240.0f, 18.0f);
	const float dx01 = tr.vertex(1).x - tr.vertex(0).x, dx12 = tr.vertex(2).x - tr.vertex(1).x;
	CHECK(near(dx12, 2.0f * dx01), "2 s gap is twice the 1 s spacing");

	// Window clipping: 100 s of 1 Hz history in a 60 s window.
	sysvitals::SeriesSnapshot<150> g;
	g.state = State::Valid; g.hasLatest = true; g.revision = 1; g.latest = 40.0f;
	for (int i = 0; i < 100; ++i) g.history[g.count++] = sysvitals::Sample{static_cast<double>(i), 40.0f, true};
	HudVitalsTrace fr;
	// 60 s window spec with the same data: older than 60 s are skipped.
	fr.update(specFps(), vitalsViewOf(g), 200.0f, 20.0f);
	CHECK(fr.vertexCount() == 61, "only the last 60 s are drawn (" + std::to_string(fr.vertexCount()) + ")");
	CHECK(near(fr.vertex(0).x, 0.0f) && near(fr.vertex(60).x, 200.0f), "window spans the plot exactly");
}

void testGapsAndBreaks() {
	auto s = makeSeries<60>({{0, 10, true}, {1, 20, true}, {2, 0, false}, {3, 30, true}, {4, 40, true}});
	HudVitalsTrace tr;
	tr.update(specCpu(), vitalsViewOf(s), 240.0f, 20.0f);
	CHECK(tr.vertexCount() == 4, "gap entry produces no vertex");
	CHECK(tr.segmentCount() == 2 && tr.segment(0).count == 2 && tr.segment(1).count == 2, "line breaks at the gap marker");
	// A gap is never plotted as a 0: no vertex sits on the baseline.
	bool anyBaseline = false;
	for (size_t i = 0; i < tr.vertexCount(); ++i) anyBaseline = anyBaseline || near(tr.vertex(i).y, 20.0f);
	CHECK(!anyBaseline, "no fabricated zero at the gap");

	// A large timestamp jump with no marker also breaks (frame-time period 0.5 s -> 2 s).
	auto j = makeSeries<120>({{0.0, 16, true}, {0.5, 16, true}, {30.0, 16, true}, {30.5, 16, true}});
	tr.update(specFt(), vitalsViewOf(j), 240.0f, 20.0f);
	CHECK(tr.segmentCount() == 2, "sampling gap > 4 periods breaks the line");

	// A single isolated valid sample is a 1-point segment (drawn as a dot).
	auto d = makeSeries<60>({{0, 5, true}, {1, 0, false}, {2, 6, true}, {3, 0, false}});
	tr.update(specCpu(), vitalsViewOf(d), 240.0f, 20.0f);
	CHECK(tr.segmentCount() == 2 && tr.segment(0).count == 1 && tr.segment(1).count == 1, "isolated points kept as 1-point segments");

	// Worst case segment capacity: alternate valid/gap across the whole RSS ring.
	sysvitals::SeriesSnapshot<300> w;
	w.state = State::Valid; w.hasLatest = true; w.revision = 1; w.latest = 1;
	for (int i = 0; i < 300; ++i) w.history[w.count++] = sysvitals::Sample{static_cast<double>(i), 100.0f + i, (i % 2) == 0};
	HudVitalsTrace big;
	big.update(vitalsSpecFor(sysvitals::Metric::RssMiB), vitalsViewOf(w), 240.0f, 20.0f);
	CHECK(big.segmentCount() == 150 && big.vertexCount() == 150, "alternating valid/gap stays within fixed capacity");
}

void testStatesAndZeros() {
	// Valid zero: drawn on the baseline, labelled 0, not treated as missing.
	sysvitals::SeriesSnapshot<60> z;
	z.state = State::Valid; z.hasLatest = true; z.latest = 0.0f; z.revision = 1;
	for (int i = 0; i < 10; ++i) z.history[z.count++] = sysvitals::Sample{static_cast<double>(i), 0.0f, true};
	HudVitalsTrace tr;
	tr.update(specCpu(), vitalsViewOf(z), 240.0f, 20.0f);
	CHECK(std::strcmp(tr.valueText(), "0 %") == 0, std::string("valid zero text: ") + tr.valueText());
	CHECK(tr.state() == State::Valid && tr.hasLatestPoint() && near(tr.latestPoint().y, 20.0f), "valid zero sits on the axis floor with a latest marker");

	// Unavailable: placeholder text, no geometry, no latest marker.
	auto u = makeSeries<150>({}, State::Unavailable, 0);
	tr.update(specGpu(), vitalsViewOf(u), 240.0f, 20.0f);
	CHECK(std::strcmp(tr.valueText(), "--") == 0 && tr.vertexCount() == 0 && !tr.hasLatestPoint() && tr.state() == State::Unavailable, "unavailable: '--', empty");
	// Unavailable-with-only-gaps history is still empty geometry.
	auto og = makeSeries<150>({{0, 0, false}, {2, 0, false}}, State::Unavailable, 3);
	tr.update(specGpu(), vitalsViewOf(og), 240.0f, 20.0f);
	CHECK(tr.vertexCount() == 0 && std::strcmp(tr.valueText(), "--") == 0, "gap-only history shows nothing, not zero");

	// Stale: last value + trace kept, but no live marker.
	auto st = makeSeries<150>({{0, 45.0f, true}, {2, 46.0f, true}}, State::Stale, 4);
	tr.update(specGpu(), vitalsViewOf(st), 240.0f, 20.0f);
	CHECK(tr.state() == State::Stale && tr.vertexCount() == 2 && !tr.hasLatestPoint(), "stale keeps the trace, drops the live marker");
	CHECK(std::strcmp(tr.valueText(), "46.0 C") == 0, std::string("stale keeps last value text: ") + tr.valueText());
}

void testAxes() {
	HudVitalsTrace tr;
	// CPU is fixed 0..100: 50% is mid-height.
	auto c = makeSeries<60>({{0, 50, true}, {1, 50, true}});
	tr.update(specCpu(), vitalsViewOf(c), 240.0f, 20.0f);
	CHECK(near(tr.axisMin(), 0.0f) && near(tr.axisMax(), 100.0f) && near(tr.vertex(0).y, 10.0f), "cpu fixed 0..100");

	// Frame time: 33.3 ms reference is visible and correctly placed; a spike widens the axis.
	auto f = makeSeries<120>({{0, 16.6f, true}, {0.5, 16.6f, true}});
	tr.update(specFt(), vitalsViewOf(f), 240.0f, 20.0f);
	CHECK(tr.hasReference() && near(tr.axisMax(), 40.0f) && near(tr.referenceY(), 20.0f * (1.0f - 33.3f / 40.0f)), "33.3 ms reference line on a 0..40 axis");
	auto spike = makeSeries<120>({{0, 16.6f, true}, {0.5, 90.0f, true}}, State::Valid, 2);
	tr.update(specFt(), vitalsViewOf(spike), 240.0f, 20.0f);
	CHECK(near(tr.axisMax(), 90.0f) && near(tr.vertex(1).y, 0.0f) && tr.hasReference(), "spike scales the axis and keeps the reference");

	// FPS axis includes 60 so 30 FPS reads as half height.
	auto p = makeSeries<120>({{0, 30, true}, {0.5, 30, true}});
	tr.update(specFps(), vitalsViewOf(p), 240.0f, 20.0f);
	CHECK(near(tr.axisMax(), 60.0f) && near(tr.vertex(0).y, 10.0f) && !tr.hasReference(), "fps axis 0..60, no reference line");

	// GPU: flat 47 C still gets a sane >= 10 C span.
	auto g = makeSeries<150>({{0, 47, true}, {2, 47, true}});
	tr.update(specGpu(), vitalsViewOf(g), 240.0f, 20.0f);
	CHECK(near(tr.axisMax() - tr.axisMin(), 10.0f) && near(tr.vertex(0).y, 10.0f), "flat temperature centred on a 10 C span");
}

void testCacheInvalidation() {
	auto a = makeSeries<60>({{0, 10, true}, {1, 20, true}}, State::Valid, 5);
	HudVitalsTrace tr;
	CHECK(tr.update(specCpu(), vitalsViewOf(a), 240.0f, 20.0f), "first update builds");
	const uint64_t n = tr.rebuildCount();
	for (int i = 0; i < 1000; ++i) tr.update(specCpu(), vitalsViewOf(a), 240.0f, 20.0f);
	CHECK(tr.rebuildCount() == n, "1000 same-revision updates rebuild nothing");
	auto b = makeSeries<60>({{0, 10, true}, {1, 20, true}, {2, 30, true}}, State::Valid, 6);
	CHECK(tr.update(specCpu(), vitalsViewOf(b), 240.0f, 20.0f) && tr.rebuildCount() == n + 1, "new series revision rebuilds");
	CHECK(tr.update(specCpu(), vitalsViewOf(b), 300.0f, 20.0f), "plot width change rebuilds");
	CHECK(tr.update(specCpu(), vitalsViewOf(b), 300.0f, 30.0f), "plot height change rebuilds");
	CHECK(!tr.update(specCpu(), vitalsViewOf(b), 300.0f, 30.0f), "unchanged layout does not");
	auto stale = b; stale.state = State::Stale;
	CHECK(tr.update(specCpu(), vitalsViewOf(stale), 300.0f, 30.0f), "state change refreshes");
	CHECK(tr.update(specFps(), vitalsViewOf(stale), 300.0f, 30.0f), "different metric spec rebuilds");

	// No heap allocation while rebuilding.
	const size_t before = g_allocs;
	for (int i = 0; i < 500; ++i) {
		auto s = b; s.revision = 100 + i;
		tr.update(specCpu(), vitalsViewOf(s), 300.0f, 30.0f);
	}
	CHECK(g_allocs == before, "trace rebuilds perform zero heap allocations (" + std::to_string(g_allocs - before) + ")");
}

// ---- Renderer integration (update path) ------------------------------------

struct FakeSource : sysvitals::ISystemSource {
	std::string stat, status, temp = "47000\n";
	int zone = 1; bool gpuPresent = true;
	size_t put(const std::string& s, char* b, size_t c) { if (s.empty() || c < 2) return 0; size_t n = std::min(s.size(), c - 1); std::memcpy(b, s.data(), n); b[n] = 0; return n; }
	size_t readProcStat(char* b, size_t c) override { return put(stat, b, c); }
	size_t readSelfStatus(char* b, size_t c) override { return put(status, b, c); }
	int findThermalZone(const char*) override { return gpuPresent ? zone : -1; }
	size_t readThermalType(int, char* b, size_t c) override { return put("gpu-thermal\n", b, c); }
	size_t readThermalTemp(int, char* b, size_t c) override { return put(temp, b, c); }
};

std::string statLine(uint64_t busy, uint64_t idle) { return "cpu  " + std::to_string(busy) + " 0 0 " + std::to_string(idle) + " 0 0 0 0 0 0\n"; }

HudFrameData frameFor(const std::string& scene, std::shared_ptr<const sysvitals::SystemVitalsSnapshot> snap) {
	HudFrameData f;
	f.scene.sceneId = scene;
	f.scene.displayName = scene;
	f.scene.health = SceneHealth::Ready;
	f.sceneManager.activeSceneId = scene;
	f.sceneManager.transitionPhase = SceneTransitionPhase::Idle;
	f.runtime.fps = 41.0f;
	f.runtime.frameTimeMs = 22.0f;
	f.systemVitals = std::move(snap);
	return f;
}

const char* kVitalIds[5] = {"universal.vitals.fps", "universal.vitals.frame_time", "universal.vitals.cpu", "universal.vitals.gpu_temp", "universal.vitals.rss"};

void testProfileAndRegions() {
	HudRegionCatalog catalog;
	HudVocabularyResolver vocab;
	HudProfileCompiler compiler(catalog, vocab);
	HudPresentationProfileRegistry registry;
	const char* scenes[] = {"blob-region-prototype", "temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair"};
	for (const char* scene : scenes) {
		auto compiled = compiler.compile(scene, registry);
		CHECK(!compiled.hasErrors(), std::string("no compile errors for ") + scene);
		int vitals = 0, inPrimary = 0, orbitUpper = 0, orbitLower = 0, flexiblePrimary = 0, statusInSummary = 0;
		for (const auto& b : compiled.bindings) {
			if (b.binding.variantId == "vitals") {
				vitals++;
				CHECK(b.valid, "vitals binding valid: " + b.binding.bindingId);
				CHECK(!b.binding.history.has_value(), "vitals binding does not use HudHistoryStore: " + b.binding.bindingId);
				CHECK(b.resolvedLabelText.find("slot.") == std::string::npos && !b.resolvedLabelText.empty(), "label resolves via vocabulary: " + b.resolvedLabelText);
				inPrimary += b.binding.regionId == "flexible.primary";
			}
			// Orbit Console v0.1 reallocation: Orbit owns #7/#8, vitals own flexible.primary (#15).
			orbitUpper += b.binding.variantId == "orbit.upper" && b.binding.regionId == "universal.activity_trace" && b.valid;
			orbitLower += b.binding.variantId == "orbit.lower" && b.binding.regionId == "universal.secondary_trace" && b.valid;
			flexiblePrimary += b.binding.regionId == "flexible.primary" && b.valid;
			// compact scene status + (optional) effect chips share universal.effect_summary
			statusInSummary += b.binding.regionId == "universal.effect_summary" && b.valid;
		}
		CHECK(vitals == 5 && inPrimary == 5 && flexiblePrimary == 5, std::string("all five vitals in flexible.primary for ") + scene);
		CHECK(orbitUpper == 1 && orbitLower == 1, std::string("one Orbit binding per trace region in ") + scene);
		const bool twoLine = std::string(scene) == "blob-region-prototype" || std::string(scene) == "fragment-trail";
		CHECK(statusInSummary == (twoLine ? 3 : 2), std::string("effect_summary hosts chips + scene status binding(s) in ") + scene);
	}
	// Canonical geometry and numbering untouched (1280x720).
	const auto* r7 = catalog.find("universal.activity_trace");
	const auto* r8 = catalog.find("universal.secondary_trace");
	CHECK(r7 && near(r7->bounds.x * 1280, 844.8f) && near(r7->bounds.y * 720, 309.6f) && near(r7->bounds.width * 1280, 409.6f) && near(r7->bounds.height * 720, 72.0f), "#7 bounds unchanged");
	CHECK(r8 && near(r8->bounds.x * 1280, 844.8f) && near(r8->bounds.y * 720, 388.8f) && near(r8->bounds.width * 1280, 409.6f) && near(r8->bounds.height * 720, 72.0f), "#8 bounds unchanged");
	CHECK(catalog.all().size() == 21, "catalog is the 20 v2 regions + controls.menu");
	CHECK(HudWireframeRenderer::regionInMinimalSlice("universal.activity_trace") && HudWireframeRenderer::regionInMinimalSlice("universal.secondary_trace"), "both trace regions are in the production set");
	CHECK(std::string(HudWireframeRenderer::productionProvenance("universal.activity_trace")) == "system_telemetry", "provenance system_telemetry");
	CHECK(HudWireframeRenderer::regionInMinimalSlice("flexible.primary") && HudWireframeRenderer::regionInMinimalSlice("universal.timing") && HudWireframeRenderer::regionInMinimalSlice("universal.effect_summary"), "#4-#6 and #15 remain in the production set");
	// No overlap with the regions above/below them.
	const auto* timing = catalog.find("universal.timing");
	const auto* primary = catalog.find("flexible.primary");
	CHECK(timing && r7->bounds.y >= timing->bounds.y + timing->bounds.height - 1e-6f, "#7 clear of universal.timing");
	CHECK(r7->bounds.y + r7->bounds.height <= r8->bounds.y + 1e-6f && r8->bounds.y + r8->bounds.height <= primary->bounds.y + 1e-6f, "#7/#8/flexible.primary stack without overlap");
}

void testRendererSnapshotIntegration() {
	FakeSource src;
	src.stat = statLine(0, 0);
	src.status = "VmRSS:\t 204800 kB\n";
	sysvitals::SystemVitals vitals(&src);
	double t = 100.0;
	auto step = [&](int frames) {
		for (int i = 0; i < frames; ++i, t += 1.0 / 60.0) {
			src.stat = statLine(static_cast<uint64_t>((t - 100.0) * 100), static_cast<uint64_t>((t - 100.0) * 300));
			vitals.update(t, 60.0f, 16.6f);
			vitals.publish(t);
		}
	};
	step(60 * 12);

	HudWireframeRenderer r;
	r.setup(1280.0f, 720.0f);
	auto frame = frameFor("blob-region-prototype", vitals.published());
	r.update(1.0f / 60.0f, frame);
	for (const char* id : kVitalIds) CHECK(r.vitalsDrawFor(id) != nullptr, std::string("vitals draw cache exists for ") + id);
	const auto* fps = r.vitalsDrawFor("universal.vitals.fps");
	const auto* cpu = r.vitalsDrawFor("universal.vitals.cpu");
	const auto* gpu = r.vitalsDrawFor("universal.vitals.gpu_temp");
	const auto* rss = r.vitalsDrawFor("universal.vitals.rss");
	CHECK(fps && near(fps->slotWidth, 81.92f) && near(fps->slotHeight, 115.2f), "flexible.primary slot = 81.92 x 115.2 (5 dial columns of 409.6)");
	CHECK(rss && near(rss->slotWidth, 81.92f) && near(rss->slotHeight, 115.2f), "all five vitals share the 81.92 px dial column");
	CHECK(fps && fps->plotW > 60.0f && fps->plotH >= 90.0f, "plot area legible (" + std::to_string(fps ? fps->plotW : 0) + " x " + std::to_string(fps ? fps->plotH : 0) + ")");
	CHECK(rss && rss->trace.state() == State::Valid && std::strcmp(rss->trace.valueText(), "200 MiB") == 0, std::string("rss value: ") + (rss ? rss->trace.valueText() : "?"));
	CHECK(cpu && cpu->trace.state() == State::Valid && std::strcmp(cpu->trace.valueText(), "25 %") == 0, std::string("cpu value: ") + (cpu ? cpu->trace.valueText() : "?"));
	CHECK(gpu && std::strcmp(gpu->trace.valueText(), "47.0 C") == 0, std::string("gpu value: ") + (gpu ? gpu->trace.valueText() : "?"));

	// Same snapshot for 200 frames: nothing rebuilds (geometry or polylines).
	const uint64_t rb = fps->polylineRebuilds, rbCpu = cpu->polylineRebuilds;
	const auto snap0 = vitals.published();
	for (int i = 0; i < 200; ++i) r.update(1.0f / 60.0f, frameFor("blob-region-prototype", snap0));
	CHECK(fps->polylineRebuilds == rb && cpu->polylineRebuilds == rbCpu, "no rebuilds while the snapshot is unchanged");

	// Per-series revisions: only series that gained data rebuild. Advance time
	// by 0.6 s: fps/frame gain a sample; gpu (0.5 Hz) may not.
	const uint64_t gpuRb = gpu->polylineRebuilds, fpsRb = fps->polylineRebuilds;
	const uint64_t gpuRevBefore = snap0->gpuTemp.revision;
	step(36);
	const auto snap1 = vitals.published();
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", snap1));
	CHECK(snap1->fps.revision != snap0->fps.revision && fps->polylineRebuilds > fpsRb, "fps trace rebuilt after a new fps sample");
	if (snap1->gpuTemp.revision == gpuRevBefore) CHECK(gpu->polylineRebuilds == gpuRb, "gpu trace NOT rebuilt when its own series is unchanged");
	else CHECK(gpu->polylineRebuilds == gpuRb + 1, "gpu trace rebuilt once for its new sample");

	// Null snapshot -> Unavailable placeholders (never zeros).
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", nullptr));
	CHECK(rss->trace.state() == State::Unavailable && std::strcmp(rss->trace.valueText(), "--") == 0 && rss->trace.vertexCount() == 0, "null snapshot -> '--', no geometry");
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", snap1));
	CHECK(rss->trace.state() == State::Valid && rss->trace.vertexCount() > 0, "snapshot returns -> trace returns");

	// Loading scene health does not affect system vitals (runtime-global).
	auto loading = frameFor("blob-region-prototype", snap1);
	loading.scene.health = SceneHealth::Loading;
	r.update(1.0f / 60.0f, loading);
	CHECK(rss->trace.state() == State::Valid && std::strcmp(rss->trace.valueText(), "200 MiB") == 0, "Loading scene: vitals unchanged");

	// Layout change (canvas resize) invalidates by layout, not revision.
	const uint64_t before = fps->trace.rebuildCount();
	r.setCanvasSize(1600.0f, 900.0f);
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", snap1));
	CHECK(fps->trace.rebuildCount() == before + 1 && near(fps->slotWidth, 102.4f), "canvas change rebuilds once at the new slot size");
	r.setCanvasSize(1280.0f, 720.0f);
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", snap1));

	// Histories are runtime-global: switching scenes must not reset them.
	const size_t verts = rss->trace.vertexCount();
	const char* scenes[] = {"temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair", "blob-region-prototype"};
	for (const char* scene : scenes) {
		r.update(1.0f / 60.0f, frameFor(scene, snap1));
		CHECK(rss->trace.vertexCount() == verts && rss->trace.state() == State::Valid, std::string("vitals continuous across scene switch to ") + scene);
		for (const char* id : kVitalIds) {
			bool valid = false;
			for (const auto& b : r.compiledProfile().bindings) if (b.binding.bindingId == id) valid = b.valid;
			CHECK(valid, std::string(id) + " compiled valid in " + scene);
		}
	}

	// Stale source: stop feeding updates; state flips to Stale on republish.
	step(0);
	double t2 = t;
	src.status = ""; src.stat = ""; src.gpuPresent = false; src.temp = "";
	for (int i = 0; i < 60 * 12; ++i, t2 += 1.0 / 60.0) { vitals.update(t2, 60.0f, 16.6f); vitals.publish(t2); }
	r.update(1.0f / 60.0f, frameFor("blob-region-prototype", vitals.published()));
	CHECK(rss->trace.state() == State::Stale && std::strcmp(rss->trace.valueText(), "200 MiB") == 0, "dead source: last value kept, state Stale");
	CHECK(cpu->trace.state() == State::Stale, "cpu Stale");
}

} // namespace


// ---- Option B: visibility-aware universal.effect_summary allocation ----------

SceneMetric numberMetric(const char* id, HudMetricValueType t, float v) {
	SceneMetric m; m.metricId = id; m.valueType = t; m.value = v; return m;
}
SceneMetric idMetric(const char* id, const char* valueId) {
	SceneMetric m; m.metricId = id; m.valueType = HudMetricValueType::Identifier; m.valueId = valueId; return m;
}

HudFrameData statusFrame(const std::string& scene, std::vector<SceneMetric> metrics, bool effectsSnapshot = false, SceneHealth health = SceneHealth::Ready) {
	HudFrameData f = frameFor(scene, nullptr);
	f.scene.health = health;
	SceneSemanticData sem;
	sem.state.primaryStateId = "scene.test.state";
	sem.metrics = std::move(metrics);
	f.scene.semantic = sem;
	if (effectsSnapshot) f.effects = videoeffects::EffectActivityStatus{};
	return f;
}

int slots(const std::string& scene, const HudFrameData& f, const char* region = "universal.effect_summary") {
	HudWireframeRenderer r;
	r.setup(1280.0f, 720.0f);
	r.update(1.0f / 60.0f, f);
	(void)scene;
	return r.regionSlotCount(region);
}

void testStatusColumnVariants() {
	HudRegionCatalog catalog; HudVocabularyResolver vocab; HudProfileCompiler compiler(catalog, vocab); HudPresentationProfileRegistry registry;
	auto variantOf = [&](const char* scene, const char* id) -> std::string {
		for (const auto& b : compiler.compile(scene, registry).bindings) if (b.binding.bindingId == id) return b.binding.variantId;
		return "(absent)";
	};
	CHECK(variantOf("fragment-trail", "fragment.status.count") == "inline.col" && variantOf("fragment-trail", "fragment.status.preset") == "inline.col", "Fragment status lines share the aligned value column");
	CHECK(variantOf("blob-region-prototype", "blob.status.regions") == "inline.col" && variantOf("blob-region-prototype", "blob.status.fragments") == "inline.col", "Blob status lines share the aligned value column");
	CHECK(variantOf("quadrant-crosshair", "quadrant.status.channels") == "grid", "Quadrant keeps the 2x2 grid");
}


// ---- CRT Phosphor three-selector rows (universal.effect_summary) --------------
void testCrtSelectorRows() {
	using T = HudMetricValueType;
	crt::Selection sel;

	// Counts are the REAL selectable options: 4 implemented modes (2 planned hidden),
	// 6 structures, 8 palettes (theme_primary_secondary hidden: no scene-accessible theme API).
	CHECK(sel.count(crt::Axis::Composition) == 4, "CRT: composition axis counts only implemented modes");
	CHECK(sel.count(crt::Axis::Structure) == 6, "CRT: six pixel structures");
	CHECK(sel.count(crt::Axis::Palette) == 8, "CRT: eight palettes (theme hidden)");
	for (const auto& id : sel.ids(crt::Axis::Composition)) CHECK(id != "contour_emission" && id != "signal_collapse", "CRT: planned modes are not selectable");
	for (const auto& id : sel.ids(crt::Axis::Palette)) CHECK(id != "theme_primary_secondary", "CRT: theme palette is hidden in the runtime");

	// Wrap in both directions; axes independent.
	crt::Selection w;
	w.step(crt::Axis::Structure, 2); w.step(crt::Axis::Palette, 3);
	w.step(crt::Axis::Composition, -1);
	CHECK(w.position(crt::Axis::Composition) == 4 && w.position(crt::Axis::Structure) == 3 && w.position(crt::Axis::Palette) == 4, "CRT: first->last wrap on one axis leaves the others untouched");
	w.step(crt::Axis::Composition, +1);
	CHECK(w.position(crt::Axis::Composition) == 1, "CRT: last->first wrap");
	CHECK(w.position(crt::Axis::Structure) == 3 && w.position(crt::Axis::Palette) == 4, "CRT: stepping composition never changes structure/palette");
	CHECK(!w.setById(crt::Axis::Palette, "theme_primary_secondary") && w.position(crt::Axis::Palette) == 4, "CRT: unknown/hidden id rejected, state unchanged");

	// Every visible name resolves through the HUD vocabulary (no raw-id leak) and fits a row.
	HudVocabularyResolver vocab;
	for (const auto& id : sel.ids(crt::Axis::Structure)) {
		crt::Selection t; t.setById(crt::Axis::Structure, id);
		const std::string key = "scene.crt.metric.structure." + id;
		CHECK(vocab.resolvedThroughVocabulary("crt-phosphor", key) && vocab.resolve("crt-phosphor", key) == t.label(crt::Axis::Structure), "CRT: structure '" + id + "' resolves to its HUD label");
		CHECK(std::string(t.label(crt::Axis::Structure)).size() <= 24, "CRT: structure label fits the row budget");
	}
	for (const auto& id : sel.ids(crt::Axis::Palette)) {
		crt::Selection t; t.setById(crt::Axis::Palette, id);
		const std::string key = "scene.crt.metric.palette." + id;
		CHECK(vocab.resolvedThroughVocabulary("crt-phosphor", key) && vocab.resolve("crt-phosphor", key) == t.label(crt::Axis::Palette), "CRT: palette '" + id + "' resolves to its HUD label");
		CHECK(std::string(t.label(crt::Axis::Palette)).size() <= 24, "CRT: palette label fits the row budget");
	}
	for (const char* k : {"scene.crt.row.pixels.label", "scene.crt.row.palette.label"})
		CHECK(vocab.resolvedThroughVocabulary("crt-phosphor", k), "CRT: row caption resolves through the vocabulary");
	// Position strings ride as Identifier values; the vocabulary's stable-ID fallback returns them verbatim.
	CHECK(vocab.resolve("crt-phosphor", "2/6") == "2/6" && !vocab.resolvedThroughVocabulary("crt-phosphor", "2/6"), "CRT: 'x/y' position text passes through the vocabulary fallback unchanged");

	auto frameWith = [&](bool metrics, bool mode, bool effects) {
		std::vector<SceneMetric> m;
		if (metrics) m = { idMetric("scene.crt.metric.structure", "scene.crt.metric.structure.rgb_triad"), idMetric("scene.crt.metric.structure_pos", "2/6"),
			idMetric("scene.crt.metric.palette", "scene.crt.metric.palette.biolum"), idMetric("scene.crt.metric.palette_pos", "5/8") };
		HudFrameData f = statusFrame("crt-phosphor", m, effects);
		if (mode) { SceneModeStatus ms; ms.modeId = "phosphor_memory"; ms.displayName = "PHOSPHOR MEMORY"; ms.index = 2; ms.count = 4; f.scene.mode = ms; }
		return f;
	};
	CHECK(slots("crt", frameWith(true, true, false)) == 2, "CRT: PIXELS + PALETTE take two slices (chips hidden); MODE is not repeated here");
	CHECK(slots("crt", frameWith(true, false, false)) == 2, "CRT: the rows do not depend on the mode status (MODE lives in media_title)");
	CHECK(slots("crt", frameWith(true, true, true)) == 3, "CRT: with an effects snapshot the chips + two rows fit (cap 4)");
	CHECK(slots("crt", frameWith(false, true, false)) == 0, "CRT: no semantic payload -> both rows hide, nothing reserved");
	CHECK(slots("crt", frameWith(false, false, false)) == 0, "CRT: unavailable scene -> nothing reserved (neutral, non-interactive)");

	// Profile compiles clean: all three bindings valid, ids stable, readonly selector variant.
	HudRegionCatalog catalog; HudProfileCompiler compiler(catalog, vocab); HudPresentationProfileRegistry registry;
	const auto compiled = compiler.compile("crt-phosphor", registry);
	int rows = 0;
	for (const auto& b : compiled.bindings) {
		if (b.binding.regionId == "universal.effect_summary" && b.binding.bindingId.rfind("crt.selector.", 0) == 0) {
			rows++;
			CHECK(b.valid && b.binding.variantId == "selector", "CRT: selector binding compiles valid with the selector variant");
			CHECK(b.binding.bindingId != "crt.selector.mode", "CRT: no MODE row (primary mode is presented once, in media_title)");
		}
	}
	CHECK(rows == 2, "CRT: exactly two selector bindings (PIXELS, PALETTE) in universal.effect_summary");
	{ // declarative opt-in: exactly one interactive SelectorGroup, and only CRT declares one
		const auto* crtProfile = registry.flexibleProfileForScene("crt-phosphor");
		CHECK(crtProfile && crtProfile->interactiveRegions.size() == 1, "CRT: declares exactly one interactive region");
		if (crtProfile && !crtProfile->interactiveRegions.empty()) {
			const auto& d = crtProfile->interactiveRegions[0];
			CHECK(d.regionId == "universal.effect_summary" && d.kind == HudInteractionKind::SelectorGroup && d.rowCount == 2, "CRT: interactive region is effect_summary, SelectorGroup, 2 rows");
		}
		for (const auto& id : registry.knownSceneIds()) {
			if (id == "crt-phosphor") continue;
			CHECK(registry.flexibleProfileForScene(id)->interactiveRegions.empty(), "passive: scene '" + id + "' declares no interactive region");
		}
		CHECK(registry.universalProfile().interactiveRegions.empty(), "passive: the universal profile declares no interactive region");
	}
	for (const char* scene : {"blob-region-prototype", "fragment-trail", "quadrant-crosshair"}) {
		int n = 0;
		for (const auto& b : compiler.compile(scene, registry).bindings) if (b.binding.regionId == "universal.effect_summary") { n++; CHECK(b.valid, "parity: other scenes' effect_summary bindings stay valid"); }
		CHECK(n <= 3, "parity: other scenes never exceed their previous three effect_summary bindings");
	}
	(void)T::Count;
}

// ---- Interactive-region style: resolved from the ACTIVE SKIN's tokens, never fixed colours ----
void testInteractionStyle() {
	using hudpresent::HudRgba8;
	using hudpresent::resolveInteractionStyle;
	auto sep = [](const HudRgba8& a, const HudRgba8& b) { float dr = float(a.r) - b.r, dg = float(a.g) - b.g, db = float(a.b) - b.b; return dr * dr + dg * dg + db * db; };
	// Dark Moss-like (light green accent, distinct secondary), a pale/light skin, and a skin whose secondary == accent.
	const HudRgba8 mossAcc{210, 240, 180, 240}, mossSec{120, 200, 120, 215}, muted{150, 190, 150, 90};
	const auto a = resolveInteractionStyle(mossAcc, mossSec, muted);
	CHECK(a.secondaryDistinct && a.active.r == mossSec.r && a.active.g == mossSec.g, "style: a distinct skin secondary becomes the ACTIVE colour");
	CHECK(a.focus.r == mossAcc.r && a.focus.g == mossAcc.g && a.focus.a >= 220, "style: FOCUS follows the skin accent");
	CHECK(a.idle.a < a.focus.a && a.idle.r == mossAcc.r, "style: IDLE is the accent at reduced strength");
	CHECK(a.disabled.r == muted.r && a.disabled.a <= 90, "style: DISABLED follows the skin muted token");
	const auto b = resolveInteractionStyle(mossAcc, mossAcc, muted); // secondary == accent (or undefined)
	CHECK(!b.secondaryDistinct && sep(b.active, b.focus) >= 25.0f * 25.0f, "style: missing/equal secondary -> derived ACTIVE is still clearly different from FOCUS");
	const HudRgba8 paleAcc{250, 245, 225, 240}; // light accent: deepened, not lifted
	const auto c = resolveInteractionStyle(paleAcc, paleAcc, muted);
	CHECK(c.active.r < paleAcc.r && c.active.g < paleAcc.g, "style: a light accent is DEEPENED for the active state");
	const HudRgba8 darkAcc{30, 60, 40, 240};
	const auto d = resolveInteractionStyle(darkAcc, darkAcc, muted);
	CHECK(d.active.g > darkAcc.g, "style: a dark accent is LIFTED for the active state");
	// Not hard-coded: two different skins give different, token-derived results.
	const auto e = resolveInteractionStyle(HudRgba8{240, 160, 60, 240}, HudRgba8{90, 120, 220, 220}, muted);
	CHECK(e.focus.r != a.focus.r && e.active.b != a.active.b, "style: changing the skin changes every state colour");
	// Non-colour cues are constants shared by all skins and must differ per state.
	const auto g = hudpresent::HudInteractionStyle::geometry();
	CHECK(g.activeStroke > g.focusStroke && g.focusCornerStroke > g.idleStroke && g.focusArm > g.idleArm, "style: stroke weight / corner size distinguish idle < focused < active without colour");
	CHECK(a.rowFill.a < 80, "style: the selected-row fill is a tint, never a flood fill");
}

void testVisibilityAwareEffectSummary() {
	using T = HudMetricValueType;
	// Blob: chips hidden (no effects snapshot) -> two status lines share the region.
	auto blob2 = statusFrame("blob-region-prototype", {numberMetric("scene.blob.metric.region_count", T::Count, 7), numberMetric("scene.blob.metric.fragment_count", T::Count, 12)});
	CHECK(slots("blob", blob2) == 2, "Blob: region + fragments lines, hidden chips reserve nothing");
	auto blob1 = statusFrame("blob-region-prototype", {numberMetric("scene.blob.metric.region_count", T::Count, 7)});
	CHECK(slots("blob", blob1) == 1, "Blob: a missing fragment_count frees its slice (one line gets the full height)");
	CHECK(slots("blob", statusFrame("blob-region-prototype", {})) == 0, "Blob: no metrics and no effects -> nothing reserved");
	// Fragment: count + preset.
	auto frag = statusFrame("fragment-trail", {numberMetric("scene.fragment.metric.fragment_count", T::Count, 17), idMetric("scene.fragment.metric.movement_preset", "scene.fragment.metric.movement_preset.hunt")});
	CHECK(slots("frag", frag) == 2, "Fragment: count + preset lines");
	CHECK(slots("frag", statusFrame("fragment-trail", {numberMetric("scene.fragment.metric.fragment_count", T::Count, 17)})) == 1, "Fragment: no preset -> one full-height line");
	// Quadrant: one 2x2 grid binding when any channel exists.
	auto quad = statusFrame("quadrant-crosshair", {idMetric("scene.quadrant.metric.channel_0_state", "scene.quadrant.state.online"), idMetric("scene.quadrant.metric.channel_1_state", "scene.quadrant.state.quiet"),
		idMetric("scene.quadrant.metric.channel_2_state", "scene.quadrant.state.quiet"), idMetric("scene.quadrant.metric.channel_3_state", "scene.quadrant.state.standby")});
	CHECK(slots("quad", quad) == 1, "Quadrant: the 2x2 grid owns the whole region");
	// Blueprint: Placeholder keeps its slot when phase_progress is absent (explicit unavailable), and when present.
	CHECK(slots("bp", statusFrame("blueprint_emergence", {})) == 1, "Blueprint: absent progress still occupies (placeholder, not hidden)");
	CHECK(slots("bp", statusFrame("blueprint_emergence", {numberMetric("scene.blueprint.metric.phase_progress", T::Ratio, 0.62f)})) == 1, "Blueprint: present progress occupies one slot");
	// Temporal: effect chips occupy the whole region only when a snapshot exists.
	CHECK(slots("tf", statusFrame("temporal-fields", {}, true)) == 1, "Temporal: present-empty effects snapshot occupies (NONE ACTIVE)");
	CHECK(slots("tf", statusFrame("temporal-fields", {}, false)) == 0, "Temporal: no snapshot -> chips hidden, no reservation");
	// Loading dims but still renders: occupancy must hold.
	auto loading = statusFrame("blob-region-prototype", {numberMetric("scene.blob.metric.region_count", T::Count, 7), numberMetric("scene.blob.metric.fragment_count", T::Count, 12)}, false, SceneHealth::Loading);
	CHECK(slots("blob", loading) == 2, "Loading: dimmed values keep their slots");
	// Untouched regions keep their allocation in every case.
	for (const auto* f : {&blob2, &quad, &loading}) {
		CHECK(slots("x", *f, "flexible.primary") == 5, "#15 still five vitals slots");
		CHECK(slots("x", *f, "universal.activity_trace") == 1 && slots("x", *f, "universal.secondary_trace") == 1, "#7/#8 Orbit slots unchanged");
		CHECK(slots("x", *f, "universal.timing") == 1, "#6 activity gauge slot unchanged");
	}
}

// ---- Orbit Console: the ratified HudFrameData::sceneColor snapshot reaches the Orbit model ----

std::shared_ptr<scenecolor::SceneColorSnapshot> colorSnap(uint64_t rev, uint64_t gen, scenecolor::State st, int n, float lin) {
	auto s = std::make_shared<scenecolor::SceneColorSnapshot>();
	s->revision = rev; s->generation = gen; s->sceneId = "blob-region-prototype"; s->state = st; s->count = uint32_t(n);
	for (int i = 0; i < n; ++i) {
		auto& x = s->history[size_t(i)];
		x.t = 100.0 + 0.5 * i; x.r = lin; x.g = lin * 0.5f; x.b = 0.0f; x.luma = 0.2126f * x.r + 0.7152f * x.g; x.valid = true;
	}
	return s;
}

void testSceneColorSnapshotReachesOrbit() {
	HudWireframeRenderer r;
	r.setup(1280.0f, 720.0f);
	// No snapshot: Unavailable, nothing invented.
	auto f = frameFor("blob-region-prototype", nullptr);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.state() == OrbitState::Unavailable && !r.orbitDraw().model.hasLatest(), "null sceneColor -> Unavailable");

	auto snap = colorSnap(1, 7, scenecolor::State::Valid, 10, 0.2158f); // linear 0.2158 == sRGB 0.5
	f.sceneColor = snap;
	r.update(1.0f / 60.0f, f);
	const auto& m = r.orbitDraw().model;
	CHECK(m.state() == OrbitState::Valid && m.hasLatest() && m.generation() == 7, "Valid snapshot -> Valid model, generation carried");
	CHECK(near(m.latest().r, 0.5f, 0.003f), "linear 0.2158 is display-encoded to ~0.5 for the swatch/orbit");
	CHECK(near(m.latest().luma, 0.2126f * 0.2158f + 0.7152f * 0.1079f, 1e-4f), "relative luminance stays LINEAR");
	CHECK(m.history().count() > 0, "history reaches the shared strips");
	// Same revision on later frames: no rebuild (cache keyed by snapshot revision).
	const uint64_t rb = r.orbitDraw().polylineRebuilds;
	for (int i = 0; i < 100; ++i) r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().polylineRebuilds == rb, "unchanged snapshot revision: geometry not rebuilt");
	// Stale: same samples, new revision, state Stale -> model Stale, still shows the last values.
	f.sceneColor = colorSnap(2, 7, scenecolor::State::Stale, 10, 0.2158f);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.state() == OrbitState::Stale && r.orbitDraw().model.hasLatest(), "Stale snapshot -> Stale model, readings retained");
	// Scene change: new generation, empty history -> Unavailable, not carried over.
	f.sceneColor = colorSnap(3, 8, scenecolor::State::Unavailable, 0, 0.0f);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.generation() == 8 && r.orbitDraw().model.state() == OrbitState::Unavailable && !r.orbitDraw().model.hasLatest(), "new generation: history cleared, no carry-over");
	// Gap sample: published invalid -> a gap, never a zero.
	auto gapSnap = colorSnap(4, 8, scenecolor::State::Valid, 4, 0.3f);
	gapSnap->history[2].valid = false;
	f.sceneColor = gapSnap;
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.history().count() > 0 && r.orbitDraw().model.history().seg(2).kind != SegKind::Valid, "invalid sample stays a gap in the shared history");
	// Resolver: runtime.scene_color is the newest valid luminance only while Valid.
	HudRealFrameResolver res;
	auto v = res.resolve("runtime.scene_color", f);
	CHECK(v && v->present && near(v->numberValue, gapSnap->history[3].luma, 1e-5f), "runtime.scene_color resolves the newest valid luminance");
	f.sceneColor = colorSnap(5, 8, scenecolor::State::Stale, 4, 0.3f);
	auto vs = res.resolve("runtime.scene_color", f);
	CHECK(vs && !vs->present, "Stale -> missing, never a live-looking reading");
	f.sceneColor = nullptr;
	auto vn = res.resolve("runtime.scene_color", f);
	CHECK(vn && !vn->present, "no snapshot -> missing");
}

// OC-003B: HudFrameData::sceneColorRaw -> the Orbit RAW strip + runtime.scene_color.raw, independent of FX.
void testRawVideoReachesOrbit() {
	HudWireframeRenderer r;
	r.setup(1280.0f, 720.0f);
	auto f = frameFor("blob-region-prototype", nullptr);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.rawState() == OrbitState::Unavailable, "null sceneColorRaw -> RAW Unavailable (NO RAW VIDEO)");

	// FX only: RAW stays unavailable and is never derived from FX.
	f.sceneColor = colorSnap(1, 7, scenecolor::State::Valid, 10, 0.2158f);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.state() == OrbitState::Valid && r.orbitDraw().model.rawState() == OrbitState::Unavailable, "FX valid, RAW absent: RAW does not borrow FX");

	// RAW published with its OWN generation/revision: both strips live, independent.
	auto raw = colorSnap(1, 3, scenecolor::State::Valid, 8, 0.0508f); // linear 0.0508 ~ sRGB 0.25
	raw->sceneId = "media.auto.1234";
	f.sceneColorRaw = raw;
	r.update(1.0f / 60.0f, f);
	const auto& m = r.orbitDraw().model;
	CHECK(m.state() == OrbitState::Valid && m.generation() == 7, "FX unchanged by RAW publication");
	CHECK(m.rawState() == OrbitState::Valid && m.raw().generation == 3, "RAW Valid with its own generation");
	CHECK(near(m.raw().latest.r, 0.25f, 0.01f) && !near(m.raw().latest.r, m.latest().r, 0.05f), "RAW colours come from the RAW snapshot, not FX");

	// RAW resets (media change) while FX keeps its history.
	const uint32_t fxCount = m.history().count();
	f.sceneColorRaw = colorSnap(2, 4, scenecolor::State::Unavailable, 0, 0.0f);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.rawState() == OrbitState::Unavailable && r.orbitDraw().model.history().count() == fxCount && r.orbitDraw().model.generation() == 7,
		"RAW generation reset leaves FX history and generation untouched");

	// RAW Stale and a gap keep source-specific semantics.
	f.sceneColorRaw = colorSnap(3, 4, scenecolor::State::Stale, 6, 0.0508f);
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.rawState() == OrbitState::Stale && r.orbitDraw().model.state() == OrbitState::Valid, "RAW Stale while FX is Valid");
	auto black = colorSnap(4, 4, scenecolor::State::Valid, 4, 0.0f); // valid black is a reading, not a gap
	black->history[0].luma = 0.0f;
	f.sceneColorRaw = black;
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.rawState() == OrbitState::Valid, "valid black RAW is Valid");

	// Unchanged RAW revision: no rebuild.
	const uint64_t rb = r.orbitDraw().polylineRebuilds;
	for (int i = 0; i < 100; ++i) r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().polylineRebuilds == rb, "unchanged RAW revision: geometry not rebuilt");

	// Disabling RAW (null) goes back to Unavailable, FX untouched.
	f.sceneColorRaw = nullptr;
	r.update(1.0f / 60.0f, f);
	CHECK(r.orbitDraw().model.rawState() == OrbitState::Unavailable && r.orbitDraw().model.state() == OrbitState::Valid, "RAW null -> Unavailable, FX untouched");

	// Resolver: runtime.scene_color.raw follows RAW only, never FX.
	HudRealFrameResolver res;
	f.sceneColor = colorSnap(9, 7, scenecolor::State::Valid, 4, 0.3f);
	f.sceneColorRaw = nullptr;
	auto none = res.resolve("runtime.scene_color.raw", f);
	CHECK(none && !none->present, "no RAW snapshot -> missing even though FX is Valid");
	auto rawSnap = colorSnap(5, 4, scenecolor::State::Valid, 4, 0.1f);
	f.sceneColorRaw = rawSnap;
	auto v = res.resolve("runtime.scene_color.raw", f);
	CHECK(v && v->present && near(v->numberValue, rawSnap->history[3].luma, 1e-5f), "runtime.scene_color.raw resolves the newest valid RAW luminance");
	auto fxv = res.resolve("runtime.scene_color", f);
	CHECK(fxv && fxv->present && near(fxv->numberValue, f.sceneColor->history[3].luma, 1e-5f), "runtime.scene_color still resolves FX");
	f.sceneColorRaw = colorSnap(6, 4, scenecolor::State::Stale, 4, 0.1f);
	auto vs = res.resolve("runtime.scene_color.raw", f);
	CHECK(vs && !vs->present, "RAW Stale -> missing, never a live-looking reading");
}

// System Performance arc dials: fill (normalised value) and health are separate;
// direction is per metric (no universal greater-is-better rule).
static void testDialRules() {
	using sysvitals::Metric;
	const auto& fps = vitalsDialSpec(Metric::Fps);
	const auto& ft = vitalsDialSpec(Metric::FrameTimeMs);
	const auto& cpu = vitalsDialSpec(Metric::CpuPercent);
	const auto& gpu = vitalsDialSpec(Metric::GpuTempC);
	const auto& rss = vitalsDialSpec(Metric::RssMiB);
	CHECK(near(vitalsDialFraction(fps, 30.0f), 0.5f) && vitalsDialFraction(fps, 200.0f) == 1.0f && vitalsDialFraction(fps, -5.0f) == 0.0f, "fps fill clamps to 0..1");
	CHECK(near(vitalsDialFraction(gpu, 60.0f), 0.5f), "gpu temp fill uses its 30..90 range");
	CHECK(vitalsDialHealth(fps, 60.0f) == VitalsHealth::Good && vitalsDialHealth(fps, 24.0f) == VitalsHealth::Ok && vitalsDialHealth(fps, 10.0f) == VitalsHealth::Bad, "high fps is healthy");
	CHECK(vitalsDialHealth(ft, 16.0f) == VitalsHealth::Good && vitalsDialHealth(ft, 40.0f) == VitalsHealth::Ok && vitalsDialHealth(ft, 60.0f) == VitalsHealth::Bad, "high frame time is unhealthy");
	CHECK(vitalsDialHealth(cpu, 30.0f) == VitalsHealth::Good && vitalsDialHealth(cpu, 95.0f) == VitalsHealth::Bad, "cpu: low good, high bad");
	CHECK(vitalsDialHealth(gpu, 50.0f) == VitalsHealth::Good && vitalsDialHealth(gpu, 85.0f) == VitalsHealth::Bad, "gpu temp: hot is bad");
	CHECK(vitalsDialHealth(rss, 300.0f) == VitalsHealth::Good && vitalsDialHealth(rss, 900.0f) == VitalsHealth::Bad, "rss: large is bad");
	// Same fill, opposite health: 50% of range for fps (30) is OK-good, for cpu (50) is good; 90% fill differs by direction.
	CHECK(vitalsDialFraction(fps, 54.0f) > 0.85f && vitalsDialHealth(fps, 54.0f) == VitalsHealth::Good, "90% fps fill is GOOD");
	CHECK(vitalsDialFraction(cpu, 90.0f) > 0.85f && vitalsDialHealth(cpu, 91.0f) == VitalsHealth::Bad, "90% cpu fill is BAD");
}

// OC-004: the region #6 segmented gauge consumes scene.visual.activity through the real renderer path:
// disabled -> unavailable (never scene.activity.overall), valid zero shows 0, smoothing, stale/transition
// -> unavailable, generation change resets.
void testVisualActivityGauge() {
	HudWireframeRenderer r;
	r.setup(1280.0f, 720.0f);
	auto mk = [](scenevisual::State st, float level, uint64_t gen) {
		auto s = std::make_shared<scenevisual::SceneVisualActivitySnapshot>();
		s->state = st; s->level = level; s->generation = gen;
		return std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot>(s);
	};
	auto gauge = [&](std::shared_ptr<const scenevisual::SceneVisualActivitySnapshot> snap, float dt = 1.0f / 30.0f) {
		auto f = frameFor("blob-region-prototype", nullptr);
		SceneSemanticData sem; sem.activity.overall = 0.77f; // must never leak into the gauge
		f.scene.semantic = sem;
		f.sceneVisualActivity = snap;
		r.update(dt, f);
		const auto* in = r.widgetInputFor("universal.activity.bar");
		return in ? in->findRole("value") : nullptr;
	};
	auto isAmbientPlaceholder = [](const hudpresent::HudWidgetRoleValue* v) {
		return v && v->shouldRender && !v->useAmbientFallback && v->value.dataClass == HudDataClass::Ambient;
	};
	auto isReal = [](const hudpresent::HudWidgetRoleValue* v, float expect) {
		return v && v->shouldRender && !v->useAmbientFallback && v->value.dataClass != HudDataClass::Ambient
			&& std::fabs(v->value.numberValue - expect) < 1e-4f;
	};
	// sampling disabled (null) -> honest unavailable, not 0.77
	CHECK(isAmbientPlaceholder(gauge(nullptr)), "disabled sampling renders the unavailable placeholder, not scene.activity.overall");
	// valid level
	CHECK(isReal(gauge(mk(scenevisual::State::Valid, 0.6f, 1)), 0.6f), "valid level drives the gauge (first observation seeds)");
	// valid zero is zero (smoothed down from 0.6 over time, then exactly decays to ~0 but remains a REAL value)
	for (int i = 0; i < 400; ++i) gauge(mk(scenevisual::State::Valid, 0.0f, 1));
	const auto* z = gauge(mk(scenevisual::State::Valid, 0.0f, 1));
	CHECK(z && z->shouldRender && z->value.dataClass != HudDataClass::Ambient && z->value.numberValue < 0.001f, "valid zero displays as real zero, not unavailable");
	// attack is fast, decay is slow
	CHECK(gauge(mk(scenevisual::State::Valid, 1.0f, 1)) && r.widgetInputFor("universal.activity.bar")->findRole("value")->value.numberValue > 0.3f, "fast attack (~34% of the gap in one 33 ms frame)");
	// stale / transition / unavailable -> placeholder (no retained lit value)
	for (auto st : {scenevisual::State::Stale, scenevisual::State::Transition, scenevisual::State::Unavailable}) {
		gauge(mk(scenevisual::State::Valid, 0.9f, 1));
		CHECK(isAmbientPlaceholder(gauge(mk(st, 0.9f, 1))), "non-valid state renders unavailable");
	}
	// generation change: no ramp from the previous scene's value
	gauge(mk(scenevisual::State::Valid, 1.0f, 1));
	CHECK(isReal(gauge(mk(scenevisual::State::Valid, 0.1f, 2)), 0.1f), "generation change resets smoothing");
}

int main() {
	testSpecs();
	testDialRules();
	testRawVideoReachesOrbit();
	testTimestampPlacement();
	testGapsAndBreaks();
	testStatesAndZeros();
	testAxes();
	testCacheInvalidation();
	testProfileAndRegions();
	testInteractionStyle();
	testRendererSnapshotIntegration();
	testVisibilityAwareEffectSummary();
	testCrtSelectorRows();
	testStatusColumnVariants();
	testSceneColorSnapshotReachesOrbit();
	testVisualActivityGauge();
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures ? 1 : 0;
}
