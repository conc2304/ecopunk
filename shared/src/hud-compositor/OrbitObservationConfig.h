#pragma once

// ============================================================================
// OrbitObservationConfig.h — OC-005 Stage 1: configurable Orbit observations (HUD Runtime).
//
// OF-free (standard library only). HUD-OWNED PRESENTATION STATE, deliberately separate from the
// telemetry snapshots: it only says WHICH measurement each of the two Orbit regions shows
//   Observation A = universal.activity_trace  (region #7, upper)
//   Observation B = universal.secondary_trace (region #8, lower)
// and never carries or alters a measurement. Defaults reproduce the previous fixed console
// (A = FX / AVG, B = RAW / AVG). Session-local only: no persistence until Architecture approves a
// configuration lifecycle.
//
// Data truth: AVG is always a production measurement (the FX and RAW average RGB series). DOM and VIB
// (OC-003, opt-in) exist only when the runtime snapshot says it measures them (OrbitSourceModel::provided);
// otherwise they are an explicit NOT AVAILABLE state. Nothing is ever substituted from AVG or from the other
// source, and DOM/VIB are available independently of each other and of AVG.
//
// OrbitConfigEditor is the small modal state machine behind the editor overlay. It is
// device-neutral: the existing HUD focus input (step ±1 / activate) and the existing back event
// drive it, so keyboard, Tab, and the physical jog wheel all use one path.
//   - open(A|B)   : the other observation's region becomes the editor; a snapshot is taken
//   - step(±1)    : moves the cursor over FX RAW AVG DOM VIB DONE (wraps)
//   - activate()  : on an option, selects it and PREVIEWS it immediately; on DONE, commits + closes
//   - cancel()    : restores the snapshot and closes
// ============================================================================

#include <array>
#include <cstdint>

namespace hudpresent {

enum class OrbitSource : uint8_t { Fx = 0, Raw = 1 };
enum class OrbitMetric : uint8_t { Avg = 0, Dom = 1, Vib = 2 };
enum class OrbitObs : uint8_t { A = 0, B = 1 };

struct OrbitObservationConfig {
	OrbitSource source = OrbitSource::Fx;
	OrbitMetric metric = OrbitMetric::Avg;
	bool operator==(const OrbitObservationConfig& o) const { return source == o.source && metric == o.metric; }
	bool operator!=(const OrbitObservationConfig& o) const { return !(*this == o); }
};

struct OrbitConsoleConfig {
	OrbitObservationConfig a { OrbitSource::Fx, OrbitMetric::Avg };
	OrbitObservationConfig b { OrbitSource::Raw, OrbitMetric::Avg };
	const OrbitObservationConfig& at(OrbitObs o) const { return o == OrbitObs::A ? a : b; }
	OrbitObservationConfig& at(OrbitObs o) { return o == OrbitObs::A ? a : b; }
	bool operator==(const OrbitConsoleConfig& o) const { return a == o.a && b == o.b; }
	bool operator!=(const OrbitConsoleConfig& o) const { return !(*this == o); }
};

// AVG is unconditionally a published measurement; DOM/VIB depend on the runtime (see OrbitSourceModel::provided).
constexpr bool orbitMetricAlwaysPublished(OrbitMetric m) { return m == OrbitMetric::Avg; }

constexpr const char* orbitSourceText(OrbitSource s) { return s == OrbitSource::Fx ? "FX" : "RAW"; }
constexpr const char* orbitMetricText(OrbitMetric m) { return m == OrbitMetric::Avg ? "AVG" : m == OrbitMetric::Dom ? "DOM" : "VIB"; }
constexpr const char* orbitObsText(OrbitObs o) { return o == OrbitObs::A ? "A" : "B"; }

// Persisted tokens (installation settings): source "fx"|"raw", metric "avg"|"dom"|"vib". The HUD maps them
// to its own enums so installation settings stay free of HUD types. Unknown token -> false (caller keeps default).
constexpr const char* orbitSourceToken(OrbitSource s) { return s == OrbitSource::Fx ? "fx" : "raw"; }
constexpr const char* orbitMetricToken(OrbitMetric m) { return m == OrbitMetric::Avg ? "avg" : m == OrbitMetric::Dom ? "dom" : "vib"; }
inline bool orbitFromTokens(const char* source, const char* metric, OrbitObservationConfig& out) {
	auto eq = [](const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return *a == *b; };
	OrbitObservationConfig c;
	if (eq(source, "fx")) c.source = OrbitSource::Fx; else if (eq(source, "raw")) c.source = OrbitSource::Raw; else return false;
	if (eq(metric, "avg")) c.metric = OrbitMetric::Avg; else if (eq(metric, "dom")) c.metric = OrbitMetric::Dom;
	else if (eq(metric, "vib")) c.metric = OrbitMetric::Vib; else return false;
	out = c;
	return true;
}

// ---- editor ---------------------------------------------------------------------------------------
enum class OrbitEditItem : uint8_t { SrcFx = 0, SrcRaw, MetAvg, MetDom, MetVib, Done };
constexpr int kOrbitEditItemCount = 6;

class OrbitConfigEditor {
public:
	enum class Result : uint8_t { None, Previewed, Committed, Cancelled };

	const OrbitConsoleConfig& config() const { return config_; } // effective (live preview while editing)
	bool isOpen() const { return open_; }
	OrbitObs target() const { return target_; }
	OrbitEditItem cursor() const { return cursor_; }
	// The observation whose region hosts the editor overlay (the one NOT being edited).
	OrbitObs overlayRegion() const { return target_ == OrbitObs::A ? OrbitObs::B : OrbitObs::A; }
	uint64_t revision() const { return revision_; } // bumps on every effective change of config()

	// Installs the committed (persisted) configuration, e.g. at startup. Ignored while an edit is open:
	// a preview is never overwritten from outside.
	bool setCommitted(const OrbitConsoleConfig& c) {
		if (open_) return false;
		if (config_ != c) {
			config_ = c;
			++revision_;
		}
		return true;
	}

	void open(OrbitObs target) {
		if (open_) return;
		open_ = true;
		target_ = target;
		snapshot_ = config_;
		const OrbitObservationConfig& c = config_.at(target);
		cursor_ = c.source == OrbitSource::Fx ? OrbitEditItem::SrcFx : OrbitEditItem::SrcRaw;
	}

	void step(int delta) {
		if (!open_ || delta == 0) return;
		int i = (int(cursor_) + delta) % kOrbitEditItemCount;
		if (i < 0) i += kOrbitEditItemCount;
		cursor_ = OrbitEditItem(i);
	}

	Result activate() {
		if (!open_) return Result::None;
		OrbitObservationConfig next = config_.at(target_);
		switch (cursor_) {
			case OrbitEditItem::SrcFx: next.source = OrbitSource::Fx; break;
			case OrbitEditItem::SrcRaw: next.source = OrbitSource::Raw; break;
			case OrbitEditItem::MetAvg: next.metric = OrbitMetric::Avg; break;
			case OrbitEditItem::MetDom: next.metric = OrbitMetric::Dom; break;
			case OrbitEditItem::MetVib: next.metric = OrbitMetric::Vib; break;
			case OrbitEditItem::Done: open_ = false; return Result::Committed;
		}
		if (next != config_.at(target_)) {
			config_.at(target_) = next;
			++revision_;
		}
		return Result::Previewed;
	}

	Result cancel() {
		if (!open_) return Result::None;
		if (config_ != snapshot_) {
			config_ = snapshot_;
			++revision_;
		}
		open_ = false;
		return Result::Cancelled;
	}

	// Is this item the currently selected option of the edited observation?
	bool selected(OrbitEditItem it) const {
		const OrbitObservationConfig& c = config_.at(target_);
		switch (it) {
			case OrbitEditItem::SrcFx: return c.source == OrbitSource::Fx;
			case OrbitEditItem::SrcRaw: return c.source == OrbitSource::Raw;
			case OrbitEditItem::MetAvg: return c.metric == OrbitMetric::Avg;
			case OrbitEditItem::MetDom: return c.metric == OrbitMetric::Dom;
			case OrbitEditItem::MetVib: return c.metric == OrbitMetric::Vib;
			case OrbitEditItem::Done: return false;
		}
		return false;
	}

private:
	OrbitConsoleConfig config_ {}, snapshot_ {};
	OrbitObs target_ = OrbitObs::A;
	OrbitEditItem cursor_ = OrbitEditItem::SrcFx;
	bool open_ = false;
	uint64_t revision_ = 0;
};

// ---- editor geometry (section-local px; the overlay replaces the whole 409.6 x 72 region) ----------
struct OrbitEditRect {
	float x = 0, y = 0, w = 0, h = 0;
};
struct OrbitEditorLayout {
	OrbitEditRect title, sourceHeader, metricHeader;
	std::array<OrbitEditRect, kOrbitEditItemCount> item {}; // indexed by OrbitEditItem; item[Done] = close action
};

inline OrbitEditorLayout orbitEditorLayout(float w, float h) {
	OrbitEditorLayout L;
	const float pad = 4.0f, gap = 4.0f;
	const float bandH = 14.0f;             // header band
	const float cellY = pad + bandH + 3.0f; // option row top
	const float cellH = h - cellY - pad;    // option row height
	// Left half: FX / RAW. Right half: AVG / DOM / VIB. Done sits in the header band, top-right.
	const float leftW = (w - 2.0f * pad) * 0.38f;
	const float rightX = pad + leftW + 10.0f;
	const float rightW = w - pad - rightX;
	const float doneW = 76.0f;
	L.title = { pad, pad, 60.0f, bandH };
	L.sourceHeader = { pad + 62.0f, pad, leftW - 62.0f, bandH };
	L.metricHeader = { rightX, pad, rightW - doneW - gap, bandH };
	const float srcW = (leftW - gap) * 0.5f;
	L.item[size_t(OrbitEditItem::SrcFx)] = { pad, cellY, srcW, cellH };
	L.item[size_t(OrbitEditItem::SrcRaw)] = { pad + srcW + gap, cellY, srcW, cellH };
	const float metW = (rightW - 2.0f * gap) / 3.0f;
	for (int i = 0; i < 3; ++i) L.item[size_t(OrbitEditItem::MetAvg) + size_t(i)] = { rightX + (metW + gap) * float(i), cellY, metW, cellH };
	L.item[size_t(OrbitEditItem::Done)] = { w - pad - doneW, pad, doneW, bandH };
	return L;
}

} // namespace hudpresent
