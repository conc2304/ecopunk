// System Telemetry v1 HUD integration — trace geometry (timestamp placement,
// gaps, states, caching) and the renderer's update path against the real
// immutable HudFrameData::systemVitals snapshot produced by the real
// sysvitals::SystemVitals collector (fake source, injected time).
//
// OF-linked (HudWireframeRenderer): no window/GL context is created; only the
// update path runs. Draw is verified in the running app by the opt-in
// SystemVitalsHudHarness.

#include "../hud-compositor/HudVitalsTrace.h"
#include "../hud-compositor/HudWireframeRenderer.h"
#include "../hud-compositor/HudProfileCompiler.h"
#include "../hud-compositor/HudPresentationProfile.h"
#include "../hud-compositor/HudRegionCatalog.h"
#include "../hud-compositor/HudVocabularyResolver.h"
#include "../hud-runtime/HudFrameData.h"
#include "SystemVitals.h"

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
		int vitals = 0, inActivity = 0, inSecondary = 0, flexiblePrimary = 0;
		for (const auto& b : compiled.bindings) {
			if (b.binding.variantId == "vitals") {
				vitals++;
				CHECK(b.valid, "vitals binding valid: " + b.binding.bindingId);
				CHECK(!b.binding.history.has_value(), "vitals binding does not use HudHistoryStore: " + b.binding.bindingId);
				CHECK(b.resolvedLabelText.find("slot.") == std::string::npos && !b.resolvedLabelText.empty(), "label resolves via vocabulary: " + b.resolvedLabelText);
				inActivity += b.binding.regionId == "universal.activity_trace";
				inSecondary += b.binding.regionId == "universal.secondary_trace";
			}
			flexiblePrimary += b.binding.regionId == "flexible.primary" && b.valid;
		}
		CHECK(vitals == 5 && inActivity == 3 && inSecondary == 2, std::string("3 + 2 distribution in ") + scene);
		CHECK(flexiblePrimary == 1, std::string("scene flexible.primary content intact in ") + scene);
	}
	// Canonical geometry and numbering untouched (1280x720).
	const auto* r7 = catalog.find("universal.activity_trace");
	const auto* r8 = catalog.find("universal.secondary_trace");
	CHECK(r7 && near(r7->bounds.x * 1280, 844.8f) && near(r7->bounds.y * 720, 309.6f) && near(r7->bounds.width * 1280, 409.6f) && near(r7->bounds.height * 720, 72.0f), "#7 bounds unchanged");
	CHECK(r8 && near(r8->bounds.x * 1280, 844.8f) && near(r8->bounds.y * 720, 388.8f) && near(r8->bounds.width * 1280, 409.6f) && near(r8->bounds.height * 720, 72.0f), "#8 bounds unchanged");
	CHECK(catalog.all().size() == 21, "catalog is the 20 v2 regions + controls.menu");
	CHECK(HudWireframeRenderer::regionInMinimalSlice("universal.activity_trace") && HudWireframeRenderer::regionInMinimalSlice("universal.secondary_trace"), "both trace regions are in the production set");
	CHECK(std::string(HudWireframeRenderer::productionProvenance("universal.activity_trace")) == "system_telemetry", "provenance system_telemetry");
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
	CHECK(fps && near(fps->slotWidth, 409.6f) && near(fps->slotHeight, 24.0f), "activity_trace slot = 409.6 x 24 (3 slots of 72)");
	CHECK(rss && near(rss->slotWidth, 409.6f) && near(rss->slotHeight, 36.0f), "secondary_trace slot = 409.6 x 36 (2 slots of 72)");
	CHECK(fps && fps->plotW > 200.0f && fps->plotH >= 16.0f, "plot area legible (" + std::to_string(fps ? fps->plotW : 0) + " x " + std::to_string(fps ? fps->plotH : 0) + ")");
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
	CHECK(fps->trace.rebuildCount() == before + 1 && near(fps->slotWidth, 512.0f), "canvas change rebuilds once at the new slot size");
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

int main() {
	testSpecs();
	testTimestampPlacement();
	testGapsAndBreaks();
	testStatesAndZeros();
	testAxes();
	testCacheInvalidation();
	testProfileAndRegions();
	testRendererSnapshotIntegration();
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures ? 1 : 0;
}
