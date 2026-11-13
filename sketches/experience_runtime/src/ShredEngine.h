#pragma once

// ShredEngine — OF-free fragment composition engine for the Shred Reconstruction scene.
//
// A machine persistently tries to rebuild the playing video from paper-shred-like cropped fragments and
// never quite succeeds. This header holds ALL composition logic (no GL, no decoders, no shaders):
//   * Fragment model (display rect, source crop + independent magnification, home/correct transform,
//     coherence, vacancy state, effect-slot assignment).
//   * 13 composition modes -> each builds a layout and emits motion/targets; none owns rendering.
//   * 10 reusable motion policies with a per-mode compatibility mask.
//   * VacancyDirector, ZoomDirector, CoherenceDirector (interruption near high coherence; hard ceiling),
//     and EffectDirector (two slots, conservative destination-area governor <= 25% of the canvas).
// The renderer (ShredReconstructionScene) only reads fragments() and draws them with batched meshes.
//
// Deterministic: one seeded RNG, fixed 1/60 s internal step (update(dt) accumulates), so the same
// seed + same dt stream replays identically.
//
// Fragment-geometry reuse note: strip/ordering ideas follow Blueprint Emergence's strip compositions
// (BEComposition) in spirit only; nothing is shared or coupled at code level (see class comment in
// ShredReconstructionScene.h).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace shred {

constexpr int kModeCount = 13;
constexpr float kEffectAreaCap = 0.25f;     // hard art contract (fraction of the canvas)
constexpr float kEffectPlanCap = 0.235f;    // planning cap: reserve against this, assert against kEffectAreaCap
constexpr float kCoherenceCeiling = 0.975f; // never-quite-correct invariant: composition coherence never exceeds this
constexpr float kFixedStep = 1.0f / 60.0f;

enum class ModeId : int { SHRED, ECHO_WAVE, SCAN, FAULT, ACCORDION, MAGNET, CONVEYOR, RECURSIVE, WEAVE, SYMMETRY, MOIRE, BLOCK, INTERRUPT };

enum class Policy : int { SNAP, GLIDE, OVERSHOOT, STEP, SWAP, PUSH_CHAIN, RIPPLE, SCATTER, MAGNETIC_PULL, JITTER, COUNT };
constexpr int kPolicyCount = int(Policy::COUNT);
inline uint32_t bit(Policy p) { return 1u << int(p); }

inline const char* policyName(Policy p) {
	static const char* n[] = {"SNAP", "GLIDE", "OVERSHOOT", "STEP", "SWAP", "PUSH_CHAIN", "RIPPLE", "SCATTER", "MAGNETIC_PULL", "JITTER"};
	return n[int(p)];
}

struct DialSpec {
	const char* label; // readable UI label
	float def;         // curated default, normalized 0..1
	float lo, hi;      // safe physical range the normalized value maps onto
	const char* unit;
};

struct ModeSpec {
	ModeId id;
	const char* key;   // stable id for telemetry / presets / tests
	const char* title; // human-readable title
	DialSpec dial[2];
	uint32_t policyMask; // compatible motion policies
	bool allowZoom;      // ZoomDirector may magnify strips
	bool allowVacancy;   // VacancyDirector may open negative space
};

inline const std::array<ModeSpec, kModeCount>& modeSpecs() {
	using P = Policy;
	static const std::array<ModeSpec, kModeCount> s = {{
		{ModeId::SHRED, "shred", "Shred / Misassembly", {{"PULL", 0.45f, 0.05f, 0.60f, "frac"}, {"MISREG", 0.40f, 1.f, 8.f, "strips"}},
			bit(P::GLIDE) | bit(P::OVERSHOOT) | bit(P::STEP) | bit(P::SNAP) | bit(P::SWAP) | bit(P::RIPPLE) | bit(P::PUSH_CHAIN) | bit(P::JITTER), true, true},
		{ModeId::ECHO_WAVE, "echo", "Echo Wave", {{"SPAN", 0.50f, 0.8f, 5.0f, "strips"}, {"DRIFT", 0.35f, 0.0f, 0.5f, "cyc/s"}},
			bit(P::GLIDE) | bit(P::STEP) | bit(P::RIPPLE) | bit(P::JITTER), false, true},
		{ModeId::SCAN, "scan", "Venetian Reconstruction", {{"WIDTH", 0.45f, 0.10f, 0.55f, "frac"}, {"RATE", 0.40f, 0.03f, 0.22f, "canvas/s"}},
			bit(P::GLIDE) | bit(P::STEP) | bit(P::SNAP) | bit(P::RIPPLE) | bit(P::JITTER), true, true},
		{ModeId::FAULT, "fault", "Fault Line", {{"THROW", 0.50f, 0.03f, 0.28f, "H"}, {"TRAVEL", 0.40f, 0.03f, 0.30f, "canvas/s"}},
			bit(P::SNAP) | bit(P::STEP) | bit(P::OVERSHOOT) | bit(P::PUSH_CHAIN) | bit(P::RIPPLE) | bit(P::GLIDE), true, true},
		{ModeId::ACCORDION, "accordion", "Accordion", {{"COMPRESS", 0.50f, 0.10f, 0.80f, "amp"}, {"BREATH", 0.40f, 0.04f, 0.45f, "Hz"}},
			bit(P::GLIDE) | bit(P::OVERSHOOT) | bit(P::RIPPLE) | bit(P::JITTER), false, true},
		{ModeId::MAGNET, "magnet", "Magnetic Sorting", {{"ATTRACT", 0.55f, 0.30f, 1.00f, "reach"}, {"CLUSTER", 0.45f, 0.15f, 0.80f, "frac"}},
			bit(P::MAGNETIC_PULL) | bit(P::OVERSHOOT) | bit(P::GLIDE) | bit(P::SWAP) | bit(P::PUSH_CHAIN) | bit(P::SCATTER) | bit(P::JITTER), true, true},
		{ModeId::CONVEYOR, "conveyor", "Cascade / Conveyor", {{"FLOW", 0.45f, 0.02f, 0.16f, "canvas/s"}, {"REROUTE", 0.40f, 0.0f, 0.60f, "prob"}},
			bit(P::GLIDE) | bit(P::STEP) | bit(P::SWAP) | bit(P::PUSH_CHAIN) | bit(P::SNAP) | bit(P::RIPPLE) | bit(P::JITTER), true, true},
		{ModeId::RECURSIVE, "recursive", "Recursive Shred", {{"NEST", 0.45f, 0.10f, 0.60f, "frac"}, {"AGITATE", 0.40f, 0.0f, 2.5f, "ev/s"}},
			bit(P::GLIDE) | bit(P::STEP) | bit(P::SNAP) | bit(P::SWAP) | bit(P::JITTER) | bit(P::SCATTER), false, true},
		{ModeId::WEAVE, "weave", "Cross-Weave", {{"BALANCE", 0.50f, 0.15f, 0.85f, "frac"}, {"DRIFT", 0.40f, 0.0f, 0.25f, "cyc/s"}},
			bit(P::GLIDE) | bit(P::STEP) | bit(P::RIPPLE) | bit(P::SNAP) | bit(P::JITTER), false, false},
		{ModeId::SYMMETRY, "symmetry", "False Symmetry", {{"MIRROR", 0.65f, 0.20f, 1.00f, "cover"}, {"LEAK", 0.35f, 0.0f, 0.5f, "ev/s"}},
			bit(P::GLIDE) | bit(P::SNAP) | bit(P::SWAP) | bit(P::JITTER) | bit(P::OVERSHOOT), false, true},
		{ModeId::MOIRE, "moire", "Moire Stack", {{"STRIPES", 0.40f, 24.f, 80.f, "strips"}, {"PHASE", 0.45f, 0.15f, 1.6f, "pitch"}},
			bit(P::GLIDE) | bit(P::JITTER) | bit(P::OVERSHOOT) | bit(P::STEP), false, false},
		{ModeId::BLOCK, "block", "Block + Shred", {{"SHARE", 0.45f, 0.05f, 0.50f, "area"}, {"TURNOVER", 0.40f, 0.03f, 0.40f, "ev/s"}},
			bit(P::GLIDE) | bit(P::OVERSHOOT) | bit(P::SNAP) | bit(P::SCATTER) | bit(P::SWAP) | bit(P::PUSH_CHAIN) | bit(P::STEP) | bit(P::RIPPLE), true, true},
		{ModeId::INTERRUPT, "interrupt", "Interrupted Reconstruction", {{"NEARNESS", 0.55f, 0.80f, 0.965f, "coh"}, {"TENSION", 0.45f, 0.3f, 6.0f, "s"}},
			bit(P::SNAP) | bit(P::SCATTER) | bit(P::GLIDE) | bit(P::OVERSHOOT) | bit(P::STEP) | bit(P::SWAP) | bit(P::PUSH_CHAIN) | bit(P::RIPPLE) | bit(P::MAGNETIC_PULL) | bit(P::JITTER), true, true},
	}};
	return s;
}
inline const ModeSpec& spec(ModeId m) { return modeSpecs()[size_t(m)]; }

// -- helpers ----------------------------------------------------------------------------------------
inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float smooth01(float t) { t = clamp01(t); return t * t * (3.f - 2.f * t); }
inline float smoother01(float t) { t = clamp01(t); return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Rng {
	uint64_t s;
	explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL) {}
	uint64_t next() {
		s += 0x9E3779B97F4A7C15ULL;
		uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
	float f() { return float(next() >> 40) * (1.0f / 16777216.0f); }
	float range(float a, float b) { return a + (b - a) * f(); }
	int irange(int a, int b) { return b <= a ? a : a + int(next() % uint64_t(b - a + 1)); }
	bool chance(float p) { return f() < p; }
	float sign() { return f() < 0.5f ? -1.f : 1.f; }
};

// Source-UV protection: shifts [u0,u1] (size <= 1) wholly inside [0,1]; never samples outside the texture.
inline void clampSpan(float& a, float& b) {
	float sz = b - a;
	if (sz >= 1.f) { a = 0.f; b = 1.f; return; }
	if (a < 0.f) { b -= a; a = 0.f; }
	if (b > 1.f) { a -= (b - 1.f); b = 1.f; }
	if (a < 0.f) a = 0.f;
}

// -- Animated channel with a motion policy ------------------------------------------------------------
struct Chan {
	float v = 0, from = 0, to = 0, t = 1, dur = 1, delay = 0, bulge = 0, reach = 1;
	float jitAmp = 0, jitPhase = 0, jitHz = 0.6f;
	Policy pol = Policy::GLIDE;
	int steps = 3;

	void set(float x) { v = from = to = x; t = 1; delay = 0; bulge = 0; reach = 1; }
	bool moving() const { return t < 1.f || delay > 0.f; }
	float target() const { return moving() ? to : v; }
	void go(float target, float d, Policy p, float dl = 0.f) {
		from = v; to = target; t = 0; dur = std::max(d, 1e-3f); delay = dl; pol = p; bulge = 0; reach = 1; steps = 3;
	}
	static float shape(Policy p, float t, int steps) {
		switch (p) {
			case Policy::OVERSHOOT: { float c1 = 1.70158f * 1.4f, c3 = c1 + 1.f; float u = t - 1.f; return 1.f + c3 * u * u * u + c1 * u * u; }
			case Policy::STEP: {
				int n = std::max(2, steps);
				float q = std::floor(t * float(n)), fr = t * float(n) - q;
				return (q + smooth01(fr * 3.f)) / float(n);
			}
			case Policy::MAGNETIC_PULL: return (1.f - std::exp(-4.f * t)) / (1.f - std::exp(-4.f));
			case Policy::SNAP: return 1.f;
			default: return smoother01(t);
		}
	}
	void step(float dt) {
		if (delay > 0.f) { delay -= dt; if (delay > 0.f) return; dt = -delay; delay = 0.f; }
		if (t >= 1.f) return;
		if (pol == Policy::SNAP) { t = 1.f; v = to; return; }
		t = std::min(1.f, t + dt / dur);
		if (t >= 1.f) { v = from + (to - from) * reach; return; }
		v = from + (to - from) * shape(pol, t, steps) * reach + bulge * std::sin(3.14159265f * t);
	}
	float out(float clock) const {
		if (jitAmp <= 0.f) return v;
		return v + jitAmp * (0.6f * std::sin(clock * 6.2831853f * jitHz + jitPhase) + 0.4f * std::sin(clock * 6.2831853f * jitHz * 2.3f + jitPhase * 1.7f));
	}
};

enum class Role : uint8_t { Base, Child, Island };
enum class Vac : uint8_t { Content, FadingOut, Vacant, FadingIn };
enum class IslandKind : uint8_t { None, Zoomed, Aligned, Vacant, Effect };

struct Fragment {
	int id = 0;
	int gen = 0;                 // generation / mode build id
	ModeId mode = ModeId::SHRED; // mode that created it
	Role role = Role::Base;
	int parent = -1;             // Child: index of parent in the same vector
	int idx = 0;                 // home index within its set

	// display geometry (canvas px): animated base + procedural additive terms (reset every step)
	Chan x, y, w, h;
	float px = 0, py = 0, pw = 0, ph = 0;
	// source crop: centre offset from the home crop centre (uv), independent magnification
	Chan cx, cy, zoom;
	float pcx = 0, pcy = 0, pzoom = 0;
	bool flipX = false, flipY = false;
	bool solid = false;          // draws opaque black (vacant island) instead of video

	// home / correct transform
	float hx = 0, hy = 0, hw = 0, hh = 0;
	float homeU = 0.5f, homeV = 0.5f;

	// vacancy: CONTENT -> FADING_OUT -> VACANT -> FADING_IN
	Vac vac = Vac::Vacant;
	float vacT = 0, vacFadeOut = 0.6f, vacHold = 0, vacFadeIn = 0.8f, vacDelay = 0;
	bool vacPending = false;     // a delayed vacate is waiting
	bool vacHoldForever = false;
	bool pendFlip = false; bool pendHas = false; float pendCx = 0, pendCy = 0; // applied when VACANT is entered
	float alpha = 0;
	float zoomHold = 0;          // seconds until a magnified fragment relaxes back

	int z = 0;
	bool drawn = true;           // false while replaced by nested children
	bool nested = false;         // parent: replaced by children
	bool ghost = false;          // belongs to the outgoing layout, fading away
	IslandKind island = IslandKind::None;
	float life = 0;              // islands: remaining lifespan

	// effect assignment: baseline is desaturated; at most ONE canonical non-default effect per fragment
	int effectSlot = -1;
	float effAlpha = 0, effTarget = 0, effRate = 1.0f, effLife = -1.f;

	float coherence = 0;
	float aux[4] = {0, 0, 0, 0}; // mode scratch

	// final, clipped, UV-protected output (filled by Engine::finalize)
	float dx = 0, dy = 0, dw = 0, dh = 0; // clipped destination rect (canvas px)
	float u0 = 0, v0 = 0, u1 = 1, v1 = 1; // source uv (flip applied: u0 may be > u1)
	float cropZoom = 1;
	bool visible = false;
};

struct EffectSlotInfo {
	std::string effectId;     // canonical id; empty = idle
	int assigned = 0;         // fragments currently holding this slot
	bool retiring = false;
	float progress = 1.f;     // 0..1 overall fade progress (entering/exiting), 1 = holding
	int pattern = 0;
};

struct Telemetry {
	float effectAreaFrac = 0;     // conservative destination-area fraction of effect fragments (<= 0.25)
	float maxEffectAreaFrac = 0;  // running max
	int effectCount = 0;          // distinct non-default effect IDs currently contributing (<= 2)
	float desaturatedFrac = 0;    // visible content area that is the desaturated baseline
	float blankFrac = 0;          // vacant / unfilled canvas fraction
	int activeFragments = 0;
	float coherence = 0;          // area-weighted composition coherence 0..1
	float maxCoherence = 0;       // running max (must stay <= kCoherenceCeiling)
	int interrupts = 0;
	int forcedEffectReleases = 0; // governor safety-net corrections (should stay 0)
	std::string phase;
	ModeId mode = ModeId::SHRED;
};

class Engine {
public:
	enum class Pattern { Sweep, CenterOut, Infection, Bands, WaveCrest, Count };

	// -- configuration ------------------------------------------------------------------------------
	void configure(int canvasW, int canvasH, bool piProfile, uint64_t seed_) {
		W = float(canvasW); H = float(canvasH); pi = piProfile; seed = seed_;
		resetDials();
		hardReset(ModeId::SHRED);
	}
	void setPiProfile(bool p) { pi = p; }
	bool piProfile() const { return pi; }

	// Reseed: new layout / motion / pacing / effect choices, SAME mode, dials unchanged.
	void reseed(uint64_t newSeed) { seed = newSeed; rebuild(mode_); }
	void hardReset(ModeId m) {
		frags.clear(); ghosts.clear();
		rng = Rng(seed);
		clock = acc = 0; gen = 0; interrupts = 0; forced = 0; maxEff = 0; maxCoh = 0;
		for (auto& s : slots) s = Slot{};
		bag.clear(); cycleTimer = rng.range(50.f, 85.f);
		vacTimer = rng.range(4.f, 9.f); zoomTimer = rng.range(6.f, 14.f);
		slots[0].idleT = rng.range(5.f, 12.f); slots[1].idleT = rng.range(12.f, 24.f);
		rebuild(m, false);
	}
	void onMediaChanged() { rebuild(mode_); } // crossfade to a fresh layout; renderer clears history

	// -- dials (per mode, normalized 0..1; persist across mode transitions; reset() restores defaults) --
	void resetDials() { for (int m = 0; m < kModeCount; ++m) for (int d = 0; d < 2; ++d) dials[m][d] = spec(ModeId(m)).dial[d].def; }
	void setDial(ModeId m, int d, float n) { if (d >= 0 && d < 2) dials[int(m)][d] = clamp01(n); }
	float dial(ModeId m, int d) const { return (d >= 0 && d < 2) ? dials[int(m)][d] : 0.f; }
	float dialPhys(ModeId m, int d) const { const DialSpec& s = spec(m).dial[d]; return lerpf(s.lo, s.hi, dials[int(m)][d]); }

	// -- modes -------------------------------------------------------------------------------------
	ModeId mode() const { return mode_; }
	void setMode(ModeId m) { if (m == mode_ && !frags.empty()) return; rebuild(m); cycleTimer = rng.range(50.f, 85.f); }
	void setAutoCycle(bool on) { autoCycle = on; }
	bool autoCycleEnabled() const { return autoCycle; }

	// -- effects -------------------------------------------------------------------------
	void setEffectCandidates(std::vector<std::string> ids) { candidates = std::move(ids); }
	void setEffectsEnabled(bool on) { effectsEnabled = on; }

	// -- stepping ----------------------------------------------------------------------------------
	void update(float dt) {
		dt = clampf(dt, 0.f, 0.25f);
		acc += dt;
		int guard = 0;
		while (acc >= kFixedStep && guard++ < 12) { acc -= kFixedStep; stepOnce(kFixedStep); }
		if (acc > kFixedStep * 12) acc = 0;
	}

	const std::vector<Fragment>& fragments() const { return frags; }
	const std::vector<Fragment>& ghostFragments() const { return ghosts; }
	const Telemetry& telemetry() const { return tel; }
	float width() const { return W; }
	float height() const { return H; }
	uint64_t currentSeed() const { return seed; }
	EffectSlotInfo slotInfo(int s) const {
		EffectSlotInfo i; const Slot& sl = slots[size_t(s)];
		i.effectId = sl.id; i.assigned = countAssigned(s); i.retiring = sl.retiring; i.pattern = sl.pattern;
		float sum = 0; int n = 0;
		for (const auto& f : frags) if (f.effectSlot == s) { sum += f.effAlpha; ++n; }
		for (const auto& f : ghosts) if (f.effectSlot == s) { sum += f.effAlpha; ++n; }
		i.progress = n ? sum / float(n) : 1.f;
		return i;
	}
	// Test / debug seams.
	void forceInterruptForTesting() { interrupt(false); }
	float computeCoherence() const { return coherenceNow(); }
	bool forceEffectForTesting(int slot, const std::string& id) { return startSlot(slot, id, Pattern::Sweep, 0.11f); }
	void clearEffectsForTesting() { for (int s = 0; s < 2; ++s) retire(s); }
	// Hand-driven stress: assigns the first `n` eligible fragments (bypasses the governor's budget on
	// purpose) so the enforcement pass can be tested.
	void forceAssignForTesting(int slot, int n) { for (auto& f : frags) { if (n-- <= 0) break; f.effectSlot = slot; f.effAlpha = 1; f.effTarget = 1; } if (slots[size_t(slot)].id.empty()) slots[size_t(slot)].id = "test_fx"; }

private:
	// ---------------------------------------------------------------------------------------------
	struct Slot {
		std::string id;
		bool active = false, retiring = false;
		float idleT = 8.f, life = 0.f, assignClock = 0.f, targetFrac = 0.08f, crest = 0.f, crestDir = 1.f;
		int pattern = 0;
		std::vector<int> queue; size_t qpos = 0;
		std::string lastId;
	};

	float W = 1280, H = 720; bool pi = false;
	uint64_t seed = 1; Rng rng{1};
	float clock = 0, acc = 0, modeClock = 0;
	int gen = 0;
	ModeId mode_ = ModeId::SHRED;
	std::vector<Fragment> frags, ghosts;
	int nextId = 1;
	float dials[kModeCount][2];
	bool autoCycle = true; std::vector<int> bag; float cycleTimer = 60.f;
	std::vector<int> prim;   // primary (base) fragment indices
	std::vector<int> order;  // dest order for packed-strip modes (fragment indices)
	Telemetry tel;
	int interrupts = 0, forced = 0; float maxEff = 0, maxCoh = 0;
	float nearT = 0, interruptCooldown = 0, phaseKick = 0;
	float vacTimer = 6, zoomTimer = 10;
	bool effectsEnabled = true; std::vector<std::string> candidates;
	Slot slots[2];

	// mode scratch state
	float ms[10] = {0}; int mi[10] = {0};
	std::string phaseName;
	Chan phaseOff;                       // generic phase channel (ECHO / FAULT / MOIRE rephase)
	struct Seam { float pos = -0.2f, vel = 0.1f, sign = 1, life = 0; bool live = false; };
	Seam seams[2];
	std::vector<Chan> lanes;             // CONVEYOR lane offsets
	std::vector<float> laneDir, laneLen;

	// ---------------------------------------------------------------------------------------------
	uint32_t mask() const { return spec(mode_).policyMask; }
	Policy pickCurve() {
		static const Policy cs[] = {Policy::SNAP, Policy::GLIDE, Policy::OVERSHOOT, Policy::STEP, Policy::SCATTER, Policy::MAGNETIC_PULL};
		static const float wt[] = {0.35f, 1.4f, 1.0f, 0.9f, 0.8f, 1.1f};
		float tot = 0; for (int i = 0; i < 6; ++i) if (mask() & bit(cs[i])) tot += wt[i];
		if (tot <= 0) return Policy::GLIDE;
		float r = rng.f() * tot;
		for (int i = 0; i < 6; ++i) if (mask() & bit(cs[i])) { r -= wt[i]; if (r <= 0) return cs[i]; }
		return Policy::GLIDE;
	}
	Policy pickOp() {
		static const Policy os[] = {Policy::SWAP, Policy::PUSH_CHAIN, Policy::RIPPLE};
		std::vector<Policy> ok; for (Policy p : os) if (mask() & bit(p)) ok.push_back(p);
		if (ok.empty()) return Policy::SWAP;
		return ok[size_t(rng.irange(0, int(ok.size()) - 1))];
	}
	// Starts a channel move honouring the policy's own shape parameters.
	void go(Chan& c, float target, float dur, Policy curve, float delay = 0.f, float scatterAmp = 0.f, float reach = 1.f) {
		c.go(target, dur, curve, delay);
		if (curve == Policy::STEP) c.steps = rng.irange(2, 5);
		if (curve == Policy::SCATTER) c.bulge = rng.sign() * rng.range(0.4f, 1.f) * scatterAmp;
		if (curve == Policy::MAGNETIC_PULL) c.reach = reach;
	}

	int addFrag(float hx, float hy, float hw, float hh, int idx) {
		Fragment f;
		f.id = nextId++; f.gen = gen; f.mode = mode_; f.idx = idx;
		f.hx = hx; f.hy = hy; f.hw = hw; f.hh = hh;
		f.homeU = (hx + hw * 0.5f) / W; f.homeV = (hy + hh * 0.5f) / H;
		f.x.set(hx); f.y.set(hy); f.w.set(hw); f.h.set(hh);
		f.cx.set(0); f.cy.set(0); f.zoom.set(1);
		f.vac = Vac::Vacant; f.alpha = 0; f.vacHold = rng.range(0.05f, 0.6f) + (hx / W) * 0.5f; f.vacFadeIn = rng.range(0.7f, 1.3f);
		frags.push_back(f);
		return int(frags.size()) - 1;
	}
	void ambientJitter(Fragment& f) {
		if (!(mask() & bit(Policy::JITTER))) return;
		f.x.jitAmp = rng.range(0.8f, 2.4f); f.y.jitAmp = rng.range(0.4f, 1.4f);
		f.x.jitPhase = rng.range(0, 6.28f); f.y.jitPhase = rng.range(0, 6.28f); f.x.jitHz = rng.range(0.25f, 0.7f); f.y.jitHz = rng.range(0.2f, 0.6f);
	}
	std::vector<float> widths(int n, float var, bool symmetric = false) {
		std::vector<float> w(static_cast<size_t>(n), 0.f); float tot = 0;
		for (int i = 0; i < n; ++i) { w[size_t(i)] = 1.f + var * (rng.f() * 2.f - 1.f); }
		if (symmetric) for (int i = 0; i < n / 2; ++i) w[size_t(n - 1 - i)] = w[size_t(i)];
		for (float v : w) tot += v;
		for (float& v : w) v = v / tot * W;
		return w;
	}
	int stripCount(int n) const { return pi ? std::max(8, int(n * 0.65f)) : n; }
	// n vertical full-height strips; fills prim + order (identity).
	void buildVertical(int n, float var, bool symmetric = false) {
		auto w = widths(n, var, symmetric); float x = 0;
		for (int i = 0; i < n; ++i) { int k = addFrag(x, 0, w[size_t(i)], H, i); frags[size_t(k)].idx = i; prim.push_back(k); order.push_back(k); x += w[size_t(i)]; ambientJitter(frags[size_t(k)]); }
	}

	// ---------------------------------------------------------------------------------------------
	void rebuild(ModeId m, bool ghostOld = true) {
		if (ghostOld) {
			for (auto& f : frags) { f.ghost = true; f.vacPending = false; f.effTarget = f.effectSlot >= 0 ? 0.f : f.effTarget; f.effRate = 1.6f; ghosts.push_back(f); }
			if (ghosts.size() > 700) ghosts.erase(ghosts.begin(), ghosts.begin() + long(ghosts.size() - 700));
		}
		frags.clear(); prim.clear(); order.clear();
		for (auto& s : slots) { s.queue.clear(); s.qpos = 0; }
		mode_ = m; ++gen; modeClock = 0; nearT = 0; interruptCooldown = 1.5f; phaseKick = 0;
		for (float& v : ms) v = 0; for (int& v : mi) v = 0;
		phaseOff = Chan{}; phaseOff.set(0);
		for (auto& s : seams) s = Seam{};
		lanes.clear(); laneDir.clear(); laneLen.clear();
		phaseName.clear();
		frags.reserve(260);
		switch (m) {
			case ModeId::SHRED: buildShred(); break;
			case ModeId::ECHO_WAVE: buildEcho(); break;
			case ModeId::SCAN: buildScan(); break;
			case ModeId::FAULT: buildFault(); break;
			case ModeId::ACCORDION: buildAccordion(); break;
			case ModeId::MAGNET: buildMagnet(); break;
			case ModeId::CONVEYOR: buildConveyor(); break;
			case ModeId::RECURSIVE: buildRecursive(); break;
			case ModeId::WEAVE: buildWeave(); break;
			case ModeId::SYMMETRY: buildSymmetry(); break;
			case ModeId::MOIRE: buildMoire(); break;
			case ModeId::BLOCK: buildBlock(); break;
			case ModeId::INTERRUPT: buildInterrupt(); break;
		}
		finalize();
	}

	// ---- shared strip-order machinery --------------------------------------------------------------
	void layoutOrder(Policy curve, Policy op, int epi, float dur) {
		float x = 0;
		for (size_t k = 0; k < order.size(); ++k) {
			Fragment& f = frags[size_t(order[k])];
			float tx = x; x += f.hw;
			if (std::fabs(f.x.target() - tx) < 0.5f) continue;
			float delay = 0.f;
			if (op == Policy::RIPPLE) delay = std::fabs(float(int(k) - epi)) * 0.06f;
			else if (op == Policy::PUSH_CHAIN) delay = std::fabs(float(int(k) - epi)) * 0.035f;
			go(f.x, tx, curve == Policy::SNAP ? 0.01f : dur, curve, delay, 0.05f * W, 0.9f);
		}
	}
	void orderSwap(int a, int b) { std::swap(order[size_t(a)], order[size_t(b)]); }
	void orderMove(int from, int to) { int v = order[size_t(from)]; order.erase(order.begin() + from); order.insert(order.begin() + to, v); }
	int orderPos(int fragIndex) const { for (size_t k = 0; k < order.size(); ++k) if (order[k] == fragIndex) return int(k); return 0; }
	void shuffleOrder(int maxDisp) {
		int n = int(order.size());
		for (int pass = 0; pass < n * 2; ++pass) { int a = rng.irange(0, n - 1); int b = clampi(a + rng.irange(-maxDisp, maxDisp), 0, n - 1); orderSwap(a, b); }
	}
	static int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
	void snapOrderNow() { // place strips at their ordered positions immediately (initial layout)
		float x = 0; for (int k : order) { frags[size_t(k)].x.set(x); x += frags[size_t(k)].hw; }
	}
	// One "sort pull" attempt: move `count` strips toward (or to) their home index.
	void sortStep(int count, float reach01, Policy curve, Policy op, float dur) {
		int n = int(order.size()); if (n < 2) return;
		std::vector<int> wrong;
		for (int k = 0; k < n; ++k) if (frags[size_t(order[size_t(k)])].idx != k) wrong.push_back(k);
		if (wrong.empty()) return;
		int epi = n / 2;
		for (int c = 0; c < count && !wrong.empty(); ++c) {
			int wi = rng.irange(0, int(wrong.size()) - 1); int k = wrong[size_t(wi)]; wrong.erase(wrong.begin() + wi);
			if (k >= n || frags[size_t(order[size_t(k)])].idx == k) continue;
			int home = frags[size_t(order[size_t(k)])].idx;
			int dest = k + int(std::lround(float(home - k) * reach01)); if (dest == k) dest = home;
			epi = k;
			if (op == Policy::PUSH_CHAIN) orderMove(k, dest); else orderSwap(k, dest);
		}
		layoutOrder(curve, op, epi, dur);
	}
	void misassemble(int disp, int count, Policy curve, Policy op, float dur) {
		int n = int(order.size()); if (n < 2) return;
		int epi = rng.irange(0, n - 1);
		for (int c = 0; c < count; ++c) {
			int a = rng.irange(0, n - 1); int b = clampi(a + (rng.chance(0.5f) ? disp : -disp), 0, n - 1); if (a == b) b = clampi(a + 1, 0, n - 1);
			epi = a;
			if (op == Policy::PUSH_CHAIN) orderMove(a, b); else orderSwap(a, b);
		}
		layoutOrder(curve, op, epi, dur);
	}
	float orderCoherenceError() const { // 0..1 mean normalized position error
		if (order.empty()) return 0; float e = 0;
		for (size_t k = 0; k < order.size(); ++k) { const Fragment& f = frags[size_t(order[k])]; e += std::fabs(f.x.target() - f.hx) / W; }
		return e / float(order.size());
	}

	// ---- BUILD: modes ------------------------------------------------------------------------------
	void buildShred() {
		buildVertical(stripCount(18), 0.55f);
		shuffleOrder(int(dialPhys(ModeId::SHRED, 1)) + 2); snapOrderNow();
		ms[0] = rng.range(1.0f, 2.2f); phaseName = "MISASSEMBLY";
	}
	void buildEcho() {
		buildVertical(stripCount(16), 0.22f);
		ms[0] = rng.range(0.f, 6.f); ms[1] = rng.range(14.f, 20.f); ms[2] = rng.range(0.f, 6.28f); phaseName = "ECHO";
	}
	void buildScan() {
		int n = pi ? 10 : 14; auto hs = widths(n, 0.5f); // 'widths' scaled to W; rescale to H
		float y = 0;
		for (int i = 0; i < n; ++i) {
			float hh = hs[size_t(i)] / W * H;
			int k = addFrag(0, y, W, hh, i); prim.push_back(k); y += hh;
			Fragment& f = frags[size_t(k)];
			f.cx.set(rng.sign() * rng.range(0.05f, 0.30f)); f.cy.set(rng.sign() * rng.range(0.f, 0.04f)); ambientJitter(f);
		}
		ms[0] = -0.3f; ms[1] = 1.f; ms[2] = 0; phaseName = "SCANNING";
	}
	void buildFault() {
		buildVertical(stripCount(22), 0.2f);
		ms[0] = rng.range(0.5f, 2.f); phaseName = "SEAM IDLE";
	}
	void buildAccordion() {
		buildVertical(stripCount(14), 0.12f);
		ms[0] = rng.range(0.f, 6.28f); ms[1] = rng.range(1.2f, 2.6f); phaseName = "BREATHING";
	}
	void buildMagnet() {
		buildVertical(stripCount(18), 0.45f);
		shuffleOrder(5); snapOrderNow();
		ms[0] = rng.range(0.8f, 1.8f); mi[0] = 0; phaseName = "REST";
	}
	void buildConveyor() {
		int L = pi ? 2 : 3, per = pi ? 7 : 10; float lh = H / float(L);
		auto w = widths(per, 0.2f); float wmax = 0; for (float v : w) wmax = std::max(wmax, v);
		float lenLane = W + wmax;
		for (int l = 0; l < L; ++l) {
			float x = 0;
			// spread strips along the lane length so a hole of ~wmax travels with them
			for (int i = 0; i < per; ++i) {
				int k = addFrag(x, l * lh, w[size_t(i)], lh, l * per + i); prim.push_back(k);
				frags[size_t(k)].aux[0] = float(l); frags[size_t(k)].aux[1] = x; ambientJitter(frags[size_t(k)]);
				x += w[size_t(i)];
			}
			Chan c; c.set(0); lanes.push_back(c); laneDir.push_back(l % 2 == 0 ? 1.f : -1.f); laneLen.push_back(lenLane);
		}
		ms[0] = 1.f; ms[1] = rng.range(7.f, 12.f); mi[0] = 0; phaseName = "FLOW";
	}
	void buildRecursive() {
		int n = stripCount(12); buildVertical(n, 0.3f); shuffleOrder(2); snapOrderNow();
		int kids = pi ? 3 : 5;
		for (int i = 0; i < n; ++i) {
			Fragment& p = frags[size_t(prim[size_t(i)])];
			for (int j = 0; j < kids; ++j) {
				int k = addFrag(p.hx, p.hy + p.hh * float(j) / float(kids), p.hw, p.hh / float(kids), 1000 + i * 10 + j);
				Fragment& c = frags[size_t(k)];
				c.role = Role::Child; c.parent = prim[size_t(i)]; c.z = 1;
				c.vac = Vac::Vacant; c.vacHoldForever = true; c.alpha = 0;
				c.zoom.set(rng.range(2.0f, 3.2f)); c.cx.set(rng.range(-0.05f, 0.05f)); c.cy.set((float(j) / float(kids) - 0.5f) * 0.25f);
			}
		}
		ms[0] = 1.5f; ms[1] = 0; phaseName = "NESTING";
	}
	void buildWeave() {
		int cols = pi ? 4 : 5, rows = 3; float tw = W / float(cols), th = H / float(rows); int ns = pi ? 3 : 4;
		for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) {
			int tile = r * cols + c;
			for (int o = 0; o < 2; ++o) for (int s = 0; s < ns; ++s) {
				float hx = c * tw + (o == 0 ? s * tw / float(ns) : 0.f), hy = r * th + (o == 1 ? s * th / float(ns) : 0.f);
				float hw = o == 0 ? tw / float(ns) : tw, hh = o == 0 ? th : th / float(ns);
				int k = addFrag(hx, hy, hw, hh, tile * 10 + o * 5 + s); prim.push_back(k);
				Fragment& f = frags[size_t(k)]; f.aux[0] = float(tile); f.aux[1] = float(o); f.vacHoldForever = true; f.vac = Vac::Vacant; f.alpha = 0; ambientJitter(f);
			}
		}
		mi[0] = cols; mi[1] = rows; mi[2] = ns; ms[0] = rng.range(0.f, 6.28f); ms[1] = 0;
		tileOwner.assign(size_t(cols * rows), -1); tileWant.assign(size_t(cols * rows), 0); tileHold.assign(size_t(cols * rows), 0.f);
		phaseName = "INTERLEAVE";
		weaveRetarget(true);
	}
	std::vector<int> tileOwner, tileWant; std::vector<float> tileHold;
	void buildSymmetry() {
		int n = stripCount(20); n += n % 2; buildVertical(n, 0.2f, true);
		ms[0] = rng.chance(0.5f) ? 1.f : 0.f; // mirrored side: 1 = right half mirrors the left
		ms[1] = rng.range(2.f, 5.f); phaseName = "MIRRORED";
		symApply(true);
	}
	void buildMoire() {
		int n = int(std::lround(dialPhys(ModeId::MOIRE, 0) / 8.f)) * 8; if (pi) n = std::min(n, 48); n = std::max(n, 24);
		mi[1] = n; buildVertical(n, 0.0f);
		for (int k : prim) { frags[size_t(k)].aux[0] = float(frags[size_t(k)].idx % 2); }
		ms[0] = rng.range(0.f, 6.28f); phaseName = "BEAT";
	}
	void buildBlock() {
		buildVertical(stripCount(20), 0.4f); shuffleOrder(2); snapOrderNow();
		ms[0] = rng.range(1.f, 3.f); phaseName = "ISLANDS";
	}
	void buildInterrupt() {
		buildVertical(stripCount(20), 0.35f); shuffleOrder(7); snapOrderNow();
		mi[0] = 0; ms[0] = rng.range(2.2f, 3.6f); ms[1] = 0; phaseName = "CHAOS";
	}

	// ---------------------------------------------------------------------------------------------
	void stepOnce(float dt) {
		clock += dt; modeClock += dt;
		for (auto& f : frags) { f.px = f.py = f.pw = f.ph = f.pcx = f.pcy = f.pzoom = 0; }
		stepMode(dt);
		for (auto& f : frags) { f.x.step(dt); f.y.step(dt); f.w.step(dt); f.h.step(dt); f.cx.step(dt); f.cy.step(dt); f.zoom.step(dt); }
		syncChildren();
		stepVacancy(dt);
		stepZoom(dt);
		finalize();
		stepCoherence(dt);
		stepEffects(dt);
		finalize();
		enforceEffectArea();
		stepGhosts(dt);
		computeTelemetry();
		autoCycleStep(dt);
	}

	// ---- per-mode step -----------------------------------------------------------------------------
	void stepMode(float dt) {
		switch (mode_) {
			case ModeId::SHRED: stepShred(dt); break;
			case ModeId::ECHO_WAVE: stepEcho(dt); break;
			case ModeId::SCAN: stepScan(dt); break;
			case ModeId::FAULT: stepFault(dt); break;
			case ModeId::ACCORDION: stepAccordion(dt); break;
			case ModeId::MAGNET: stepMagnet(dt); break;
			case ModeId::CONVEYOR: stepConveyor(dt); break;
			case ModeId::RECURSIVE: stepRecursive(dt); break;
			case ModeId::WEAVE: stepWeave(dt); break;
			case ModeId::SYMMETRY: stepSymmetry(dt); break;
			case ModeId::MOIRE: stepMoire(dt); break;
			case ModeId::BLOCK: stepBlock(dt); break;
			case ModeId::INTERRUPT: stepInterruptMode(dt); break;
		}
		phaseOff.step(dt);
	}

	void stepShred(float dt) {
		ms[0] -= dt; if (ms[0] > 0) return;
		float pull = dialPhys(ModeId::SHRED, 0); int disp = int(std::lround(dialPhys(ModeId::SHRED, 1)));
		int n = int(order.size()); Policy curve = pickCurve(), op = pickOp();
		float dur = rng.range(0.7f, 1.6f);
		float C = coherenceNow();
		if (C < 0.80f && rng.chance(0.78f)) sortStep(std::max(1, int(std::lround(pull * float(n) * 0.6f))), rng.range(0.6f, 1.f), curve, op, dur);
		else if (C < 0.9f && rng.chance(0.6f)) sortStep(std::max(1, int(std::lround(pull * float(n) * 0.3f))), 1.f, curve, op, dur);
		else {
			misassemble(disp, rng.irange(1, 2), curve, op, dur);
			if (rng.chance(0.25f + 0.4f * dials[0][1])) { Fragment& f = frags[size_t(order[size_t(rng.irange(0, n - 1))])]; go(f.cx, rng.sign() * rng.range(0.03f, 0.03f + 0.01f * float(disp)), dur, curve, 0, 0.02f); go(f.cx, 0.f, dur, Policy::GLIDE, dur + rng.range(2.f, 5.f)); }
		}
		ms[0] = rng.chance(0.3f) ? rng.range(3.f, 6.5f) : rng.range(0.8f, 2.4f); // restful pauses
	}

	void stepEcho(float dt) {
		float span = dialPhys(ModeId::ECHO_WAVE, 0), drift = dialPhys(ModeId::ECHO_WAVE, 1);
		ms[2] += drift * dt;
		float env = 0.5f + 0.5f * std::sin(6.2831853f * modeClock / ms[1] + ms[0]);
		float A = span * (0.12f + 0.88f * env);
		float waves = 1.4f + 2.2f * dials[1][0];
		int n = int(prim.size());
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			float s = float(i) / float(std::max(1, n - 1));
			float u = std::sin(6.2831853f * (s * waves + ms[2] + phaseOff.v));
			float q = A * u, fl = std::floor(q), fr = q - fl;
			float shiftStrips = fl + smooth01((fr - 0.35f) / 0.3f);
			f.pcx = shiftStrips * (f.hw / W);
		}
	}

	void stepScan(float dt) {
		float width = dialPhys(ModeId::SCAN, 0), rate = dialPhys(ModeId::SCAN, 1);
		ms[0] += ms[1] * rate * dt; // band centre in canvas fraction (travels beyond both edges)
		float half = width * 0.5f;
		if ((ms[1] > 0 && ms[0] > 1.f + half + 0.05f) || (ms[1] < 0 && ms[0] < -half - 0.05f)) {
			ms[2] += dt; // pause off-screen (fully misaligned), then re-roll offsets and sweep the other way
			if (ms[2] > rng.range(0.8f, 3.2f)) {
				ms[2] = 0; ms[1] = -ms[1];
				for (int k : prim) { Fragment& f = frags[size_t(k)]; go(f.cx, rng.sign() * rng.range(0.05f, 0.30f), rng.range(1.f, 2.f), pickCurve(), rng.range(0.f, 1.f), 0.04f); go(f.cy, rng.sign() * rng.range(0.f, 0.04f), 1.4f, Policy::GLIDE); }
			}
		}
		int n = int(prim.size());
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			float c = (f.hy + f.hh * 0.5f) / H;
			float d = std::fabs(c - ms[0]) / std::max(0.02f, half);
			float r = (1.f - smooth01((d - 0.55f) / 0.45f)) * 0.975f; // flat coherent core, soft shoulders
			f.pcx = -f.cx.v * r; f.pcy = -f.cy.v * r;
		}
	}

	void stepFault(float dt) {
		float thr = dialPhys(ModeId::FAULT, 0), speed = dialPhys(ModeId::FAULT, 1);
		ms[0] -= dt;
		bool any = false; for (auto& s : seams) any = any || s.live;
		if (!any && ms[0] <= 0) spawnSeam(speed);
		if (any && modeClock > 14.f && ms[0] <= 0 && rng.chance(0.4f * dt)) spawnSeam(speed);
		for (auto& s : seams) if (s.live) { s.pos += s.vel * dt; if ((s.vel > 0 && s.pos > 1.15f) || (s.vel < 0 && s.pos < -0.15f)) { s.live = false; ms[0] = rng.range(0.4f, 3.0f); } }
		phaseName = any ? "SEAM TRAVEL" : "SEAM IDLE";
		int n = int(prim.size());
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			float cxn = (f.hx + f.hw * 0.5f) / W, off = 0;
			for (auto& s : seams) if (s.live) off += s.sign * (1.f - smooth01((cxn - s.pos) / 0.045f + 0.5f)); // strips behind the seam
			f.pcy = off * thr * 0.9f; f.py = off * thr * H * 0.35f; f.pcx = off * thr * 0.12f;
		}
	}
	void spawnSeam(float speed) {
		for (auto& s : seams) if (!s.live) {
			bool right = rng.chance(0.5f); s.live = true; s.pos = right ? -0.08f : 1.08f; s.vel = (right ? 1.f : -1.f) * speed * rng.range(0.8f, 1.25f); s.sign = rng.sign();
			return;
		}
	}

	void stepAccordion(float dt) {
		float comp = dialPhys(ModeId::ACCORDION, 0), rate = dialPhys(ModeId::ACCORDION, 1);
		ms[0] += 6.2831853f * rate * dt;
		int n = int(prim.size()); std::vector<float> wt(static_cast<size_t>(n), 0.f); float tot = 0;
		for (int i = 0; i < n; ++i) {
			float s = float(i) / float(n);
			wt[size_t(i)] = frags[size_t(prim[size_t(i)])].hw * (1.f + comp * std::sin(6.2831853f * s * ms[1] + ms[0]) * 0.85f + comp * 0.35f * std::sin(6.2831853f * s * ms[1] * 2.3f - ms[0] * 1.3f));
			wt[size_t(i)] = std::max(wt[size_t(i)], frags[size_t(prim[size_t(i)])].hw * 0.12f); tot += wt[size_t(i)];
		}
		float x = 0;
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			float wi = wt[size_t(i)] / tot * W, r = wi / f.hw;
			f.x.set(x); f.w.set(wi); x += wi;
			f.zoom.set(r < 1.f ? 1.f + (1.f - r) * (1.0f + 3.0f * comp) : 1.f); // compressed band => magnified source
		}
	}

	void stepMagnet(float dt) {
		float attract = dialPhys(ModeId::MAGNET, 0), frac = dialPhys(ModeId::MAGNET, 1);
		int n = int(order.size());
		ms[0] -= dt; if (ms[0] > 0) { return; }
		Policy curve = pickCurve();
		int m = std::max(3, int(std::lround(frac * float(n))));
		switch (mi[0]) {
			case 0: { // PULL: a cluster seeks home, asymptotically, possibly interrupted short of arrival
				mi[1] = rng.irange(0, std::max(0, n - m)); mi[2] = m; phaseName = "PULL";
				std::vector<int> ids; for (int k = mi[1]; k < mi[1] + m && k < n; ++k) ids.push_back(order[size_t(k)]);
				for (int id : ids) {
					int k = orderPos(id), home = frags[size_t(id)].idx; int dest = k + int(std::lround(float(home - k) * attract));
					if (dest != k) { if (rng.chance(0.5f)) orderMove(k, dest); else orderSwap(k, dest); }
				}
				layoutOrder(mask() & bit(Policy::MAGNETIC_PULL) ? Policy::MAGNETIC_PULL : curve, Policy::PUSH_CHAIN, mi[1], rng.range(1.0f, 1.8f));
				for (int id : ids) frags[size_t(id)].x.reach = clampf(attract * rng.range(0.85f, 1.05f), 0.3f, 1.f);
				mi[0] = 1; ms[0] = rng.range(1.2f, 2.0f); break; }
			case 1: { // OVERSHOOT / REPEL: cluster members are nudged apart, then relax
				phaseName = "REPEL";
				for (int k = mi[1]; k < mi[1] + mi[2] && k < n; ++k) { Fragment& f = frags[size_t(order[size_t(k)])]; float dir = (float(k - mi[1]) / float(std::max(1, mi[2] - 1)) - 0.5f) * 2.f; go(f.x, f.x.target() + dir * rng.range(0.012f, 0.035f) * W, 0.5f, Policy::OVERSHOOT); }
				mi[0] = 2; ms[0] = 0.7f; break; }
			case 2: { // RELEASE: cluster re-misaligns
				phaseName = "RELEASE";
				misassemble(rng.irange(2, 6), 2, curve, mask() & bit(Policy::SWAP) ? Policy::SWAP : Policy::PUSH_CHAIN, rng.range(0.8f, 1.5f));
				layoutOrder(Policy::GLIDE, Policy::SWAP, 0, 1.0f);
				mi[0] = 0; ms[0] = rng.chance(0.35f) ? rng.range(3.f, 6.f) : rng.range(0.8f, 2.2f); phaseName = "REST"; break; }
		}
	}

	void stepConveyor(float dt) {
		float speed = dialPhys(ModeId::CONVEYOR, 0), reroute = dialPhys(ModeId::CONVEYOR, 1);
		// intermittent sorting pause
		ms[1] -= dt;
		if (mi[0] == 0 && ms[1] <= 0) { mi[0] = 1; ms[2] = 0; phaseName = "SORT PAUSE"; }
		if (mi[0] == 1) {
			ms[0] = std::max(0.f, ms[0] - dt / 1.2f); ms[2] += dt;
			if (ms[2] > 1.3f && ms[2] - dt <= 1.3f) { for (size_t l = 0; l < lanes.size(); ++l) { float L = laneLen[l]; float tgt = std::round(lanes[l].v / L) * L; lanes[l].go(tgt, 1.8f, Policy::GLIDE); } }
			if (ms[2] > 4.8f) { mi[0] = 2; ms[2] = 0; }
		} else if (mi[0] == 2) { ms[0] = std::min(1.f, ms[0] + dt / 1.5f); if (ms[2] += dt, ms[2] > 1.6f) { mi[0] = 0; ms[1] = rng.range(8.f, 15.f); phaseName = "FLOW"; } }
		for (size_t l = 0; l < lanes.size(); ++l) {
			Chan& c = lanes[l];
			if (c.moving()) c.step(dt); else if (mi[0] != 1) c.v += laneDir[l] * speed * W * ms[0] * dt;
		}
		// reroute: exchange lane identity of two strips
		ms[3] -= dt;
		if (ms[3] <= 0 && reroute > 0.01f && mi[0] != 1) {
			ms[3] = rng.range(1.2f, 3.5f) / std::max(0.2f, reroute * 2.f);
			if (rng.chance(reroute * 1.4f) && prim.size() > 3) {
				Fragment& a = frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])]; Fragment& b = frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])];
				if (a.aux[0] != b.aux[0]) { float la = a.aux[0], lb = b.aux[0]; float lh = H / float(lanes.size()); Policy cu = pickCurve(); go(a.y, lb * lh, 0.9f, cu, 0, lh * 0.3f); go(b.y, la * lh, 0.9f, cu, 0.1f, lh * 0.3f); a.aux[0] = lb; b.aux[0] = la; }
			}
		}
		for (int k : prim) {
			Fragment& f = frags[size_t(k)]; int homeLane = int(std::lround(f.hy / (H / float(lanes.size()))));
			float L = laneLen[size_t(homeLane)]; float off = lanes[size_t(homeLane)].v;
			float wmax = L - W; float xx = f.hx + off;
			xx = std::fmod(xx + wmax, L); if (xx < 0) xx += L; xx -= wmax;
			f.x.set(xx);
		}
	}

	void stepRecursive(float dt) {
		float extent = dialPhys(ModeId::RECURSIVE, 0), motion = dialPhys(ModeId::RECURSIVE, 1);
		int n = int(prim.size());
		// occasional mild strip rearrangement (so the base field is a living shred, never idle)
		ms[3] -= dt; if (ms[3] <= 0) { Policy cu = pickCurve(), op = pickOp(); if (coherenceNow() < 0.8f && rng.chance(0.5f)) sortStep(2, 1.f, cu, op, 1.2f); else misassemble(2, 1, cu, op, 1.2f); ms[3] = rng.range(2.f, 6.f); }
		// nest rotation
		ms[0] -= dt;
		if (ms[0] <= 0) {
			for (int i = 0; i < n; ++i) setNested(prim[size_t(i)], false);
			int want = std::max(1, int(std::lround(extent * float(n)))), c0 = rng.irange(0, std::max(0, n - want));
			bool contiguous = rng.chance(0.55f); std::vector<int> pick;
			for (int i = 0; i < n; ++i) pick.push_back(i);
			for (int i = n - 1; i > 0; --i) std::swap(pick[size_t(i)], pick[size_t(rng.irange(0, i))]);
			for (int j = 0; j < want; ++j) { int i = contiguous ? c0 + j : pick[size_t(j)]; if (i < n) setNested(order[size_t(i)], true); }
			ms[0] = rng.range(8.f, 14.f);
		}
		// inner motion: agitate the nested sub-strips (crop offsets exchange / jump); zero = still
		ms[1] -= dt;
		if (motion > 0.01f && ms[1] <= 0) {
			ms[1] = rng.range(0.4f, 1.f) / motion;
			for (int tries = 0; tries < 3; ++tries) {
				int pi_ = prim[size_t(rng.irange(0, n - 1))]; if (!frags[size_t(pi_)].nested) continue;
				std::vector<int> kids; for (size_t k = 0; k < frags.size(); ++k) if (frags[k].parent == pi_) kids.push_back(int(k));
				if (kids.size() < 2) continue;
				int a = kids[size_t(rng.irange(0, int(kids.size()) - 1))], b = kids[size_t(rng.irange(0, int(kids.size()) - 1))];
				Policy cu = rng.chance(0.5f) ? Policy::STEP : pickCurve();
				if (a != b && (mask() & bit(Policy::SWAP)) && rng.chance(0.5f)) { float ya = frags[size_t(a)].cy.target(), yb = frags[size_t(b)].cy.target(); go(frags[size_t(a)].cy, yb, 0.5f, cu); go(frags[size_t(b)].cy, ya, 0.5f, cu); }
				else { go(frags[size_t(a)].cx, rng.range(-0.08f, 0.08f), 0.6f, cu, 0, 0.03f); go(frags[size_t(a)].zoom, rng.range(1.9f, 3.4f), 0.8f, Policy::GLIDE); }
				break;
			}
		}
		phaseName = "NESTING";
	}
	void setNested(int parentIdx, bool on) {
		Fragment& p = frags[size_t(parentIdx)];
		if (p.nested == on) return;
		p.nested = on;
		if (on) { p.drawn = true; vacate(p, 0.f, 0.5f, 9999.f, 0.8f, true); } // parent leaves; children enter once it is vacant (syncChildren)
		else vacate(p, 0.f, 0.01f, 0.5f, 0.8f, false);
		if (on && p.effectSlot >= 0) { p.effTarget = 0; }
	}
	void syncChildren() {
		if (mode_ != ModeId::RECURSIVE) return;
		for (size_t k = 0; k < frags.size(); ++k) {
			Fragment& c = frags[k]; if (c.role != Role::Child) continue;
			Fragment& p = frags[size_t(c.parent)];
			// geometry follows the parent's current (animated) rectangle
			float px_ = p.x.out(clock) + p.px, pw_ = p.w.v + p.pw; float kids = float(pi ? 3 : 5);
			int j = c.idx % 10;
			c.x.set(px_); c.w.set(pw_); c.y.set(p.hy + p.hh * float(j) / kids); c.h.set(p.hh / kids);
			c.homeU = p.homeU; c.homeV = p.homeV; c.hx = px_; c.hy = c.y.v; c.hw = pw_; c.hh = c.h.v;
			bool parentGone = p.nested && p.vac == Vac::Vacant && p.alpha <= 0.001f;
			if (p.nested && parentGone && c.vac == Vac::Vacant && c.vacHoldForever) { c.vacHoldForever = false; c.vacHold = rng.range(0.f, 0.4f); c.vacFadeIn = rng.range(0.6f, 1.0f); }
			if (!p.nested && (c.vac == Vac::Content || c.vac == Vac::FadingIn) && !c.vacPending) { c.vacHoldForever = true; vacate(c, 0.f, 0.5f, 9999.f, 0.7f, true); }
		}
	}

	void weaveRetarget(bool initial) {
		float bal = dialPhys(ModeId::WEAVE, 0); int cols = mi[0], rows = mi[1];
		for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) {
			int t = r * cols + c;
			float fld = 0.5f + 0.5f * std::sin(6.2831853f * (0.9f * float(c) / float(cols) + 0.6f * float(r) / float(rows)) - ms[0] + 0.6f * std::sin(float(t) * 1.7f));
			int want = fld < bal ? 1 : 0; // 1 = horizontal strips, 0 = vertical
			if (initial) { tileOwner[size_t(t)] = -1; tileWant[size_t(t)] = want; tileHold[size_t(t)] = 0; }
			else if (want != tileWant[size_t(t)]) { tileWant[size_t(t)] = want; tileHold[size_t(t)] = 0; }
			else tileHold[size_t(t)] += 0.5f;
			if (tileOwner[size_t(t)] != want && (initial || tileHold[size_t(t)] >= 2.0f)) {
				for (int k : prim) {
					Fragment& f = frags[size_t(k)]; if (int(f.aux[0]) != t) continue;
					bool mine = int(f.aux[1]) == want;
					if (mine) { f.vacHoldForever = false; if (f.vac == Vac::Vacant) { f.vacHold = initial ? rng.range(0.f, 0.6f) : rng.range(0.55f, 0.9f) + 0.15f * float(c); f.vacT = 0; f.vacFadeIn = rng.range(0.6f, 0.9f); } }
					else if (f.vac != Vac::Vacant) vacate(f, 0.f, 0.5f, 9999.f, 0.7f, true);
				}
				tileOwner[size_t(t)] = want;
			}
		}
	}
	void stepWeave(float dt) {
		float drift = dialPhys(ModeId::WEAVE, 1);
		ms[0] += 6.2831853f * drift * dt;
		ms[1] -= dt; if (ms[1] <= 0) { ms[1] = 0.5f; weaveRetarget(false); }
		// crop misregistration events inside tiles
		ms[2] -= dt;
		if (ms[2] <= 0) {
			ms[2] = rng.range(0.5f, 1.4f); int ns = mi[2];
			for (int tries = 0; tries < 2; ++tries) {
				Fragment& f = frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])]; if (f.vac != Vac::Content) continue;
				bool horiz = int(f.aux[1]) == 1; float unit = horiz ? f.hh / H : f.hw / W; Policy cu = pickCurve();
				float tgt = rng.chance(0.35f) ? 0.f : rng.sign() * unit * float(rng.irange(1, std::max(1, ns - 1)));
				go(horiz ? f.cy : f.cx, tgt, rng.range(0.6f, 1.4f), cu, 0, unit * 0.5f);
			}
		}
	}

	void symApply(bool initial) {
		float bias = dialPhys(ModeId::SYMMETRY, 0); int n = int(prim.size()); int half = n / 2;
		bool rightMirrors = ms[0] > 0.5f;
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			bool onMirrorSide = rightMirrors ? i >= half : i < half;
			int distFromCenter = rightMirrors ? i - half : half - 1 - i; // 0 = nearest the axis
			bool want = onMirrorSide && float(distFromCenter) < bias * float(half) - 0.001f + (bias >= 0.999f ? 1.f : 0.f);
			float c = f.hx + f.hw * 0.5f;
			f.aux[2] = want ? 1.f : 0.f;
			float tcx = want ? (W - 2.f * c) / W : 0.f;
			if (initial) { f.cx.set(tcx); f.flipX = want; }
			else if (f.flipX != want) { f.pendHas = true; f.pendFlip = want; f.pendCx = tcx; f.pendCy = 0; vacate(f, rng.range(0.f, 0.6f), 0.4f, 0.05f, 0.5f, false); }
		}
	}
	void stepSymmetry(float dt) {
		float leak = dialPhys(ModeId::SYMMETRY, 1);
		static float lastBias = -1; (void)lastBias;
		ms[3] -= dt; if (ms[3] <= 0) { ms[3] = 1.0f; symRefreshIfBiasChanged(); }
		ms[1] -= dt;
		if (ms[1] <= 0) { // slow re-roll of which side mirrors
			ms[1] = rng.range(18.f, 32.f); if (modeClock > 1.f) { ms[0] = ms[0] > 0.5f ? 0.f : 1.f; symApply(false); }
		}
		ms[4] -= dt;
		if (leak > 0.01f && ms[4] <= 0) {
			ms[4] = rng.range(0.6f, 1.6f) / leak;
			for (int tries = 0; tries < 4; ++tries) { // one strip betrays its true source
				Fragment& f = frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])];
				if (f.aux[2] < 0.5f || f.vac != Vac::Content) continue;
				float c = f.hx + f.hw * 0.5f, mirror = (W - 2.f * c) / W;
				f.pendHas = true; f.pendFlip = false; f.pendCx = 0; f.pendCy = 0; f.aux[3] = rng.range(1.2f, 3.0f); // seconds of truth
				vacate(f, 0.f, 0.3f, 0.04f, 0.4f, false); (void)mirror; f.aux[2] = 2.f; break;
			}
		}
		for (int k : prim) { Fragment& f = frags[size_t(k)]; if (f.aux[2] > 1.5f) { f.aux[3] -= dt; if (f.aux[3] <= 0 && f.vac == Vac::Content) { float c = f.hx + f.hw * 0.5f; f.pendHas = true; f.pendFlip = true; f.pendCx = (W - 2.f * c) / W; f.pendCy = 0; vacate(f, 0.f, 0.3f, 0.04f, 0.4f, false); f.aux[2] = 1.f; } } }
	}
	float lastSymBias = -1.f;
	void symRefreshIfBiasChanged() { float b = dialPhys(ModeId::SYMMETRY, 0); if (std::fabs(b - lastSymBias) > 0.02f) { lastSymBias = b; if (modeClock > 0.5f) symApply(false); } }

	void stepMoire(float dt) {
		float amp = dialPhys(ModeId::MOIRE, 1); int n = int(prim.size());
		int want = int(std::lround(dialPhys(ModeId::MOIRE, 0) / 8.f)) * 8; if (pi) want = std::min(want, 48); want = std::max(want, 24);
		if (want != mi[1] && modeClock > 1.5f) { rebuild(ModeId::MOIRE); return; } // pitch change = cheap relayout crossfade
		ms[0] += 6.2831853f * 0.07f * dt;
		float phi = amp * std::sin(ms[0]) + phaseOff.v;
		float pitch = W / float(n) * 2.f; // lattice pitch = two strips
		for (int i = 0; i < n; ++i) {
			Fragment& f = frags[size_t(prim[size_t(i)])];
			float chirp = 1.f + 0.7f * float(i) / float(n);
			float off = f.aux[0] < 0.5f ? 0.5f * phi * pitch : -0.5f * phi * pitch * chirp;
			f.pcx = off / W; f.pzoom = f.aux[0] < 0.5f ? 0.f : 0.012f * (1.f + std::sin(ms[0] * 1.7f));
		}
	}

	// BLOCK: islands over a strip field
	void stepBlock(float dt) {
		float share = dialPhys(ModeId::BLOCK, 0), turnover = dialPhys(ModeId::BLOCK, 1);
		ms[3] -= dt; if (ms[3] <= 0) { Policy cu = pickCurve(), op = pickOp(); if (coherenceNow() < 0.82f && rng.chance(0.5f)) sortStep(2, 1.f, cu, op, 1.2f); else misassemble(2, 1, cu, op, 1.2f); ms[3] = rng.range(2.5f, 7.f); }
		float cover = 0; int live = 0;
		for (size_t k = 0; k < frags.size(); ++k) { Fragment& f = frags[k]; if (f.role != Role::Island) continue; if (f.vac != Vac::Vacant || f.vacHoldForever) { cover += f.dw * f.dh; } if (f.vacHoldForever) continue; ++live;
			f.life -= dt; if (f.life <= 0 && f.vac == Vac::Content) { vacate(f, 0.f, rng.range(0.5f, 0.9f), 9999.f, 0.f, true); f.aux[0] = 1.f; } }
		// retire fully-vanished islands (vector indices of prim/order are unaffected: islands are appended after)
		ms[0] -= dt;
		float target = share * W * H; int maxIslands = pi ? 2 : 5;
		if (ms[0] <= 0) {
			int active = 0; float area = 0;
			for (auto& f : frags) if (f.role == Role::Island && !f.vacHoldForever) { ++active; area += f.hw * f.hh; }
			if (area < target * 0.92f && active < maxIslands) spawnIsland(target - area);
			ms[0] = rng.range(0.7f, 1.6f) / std::max(0.2f, turnover * 2.5f);
		}
		(void)cover; (void)live;
	}
	void spawnIsland(float roomArea) {
		const float cw = W / 8.f, ch = H / 6.f;
		int gw = rng.irange(2, 4), gh = rng.irange(2, 3);
		while (float(gw * gh) * cw * ch > roomArea * 1.25f && gw * gh > 4) { if (gw > gh) --gw; else --gh; }
		for (int attempt = 0; attempt < 6; ++attempt) {
			int gx = rng.irange(0, 8 - gw), gy = rng.irange(0, 6 - gh);
			float rx = gx * cw, ry = gy * ch, rw = gw * cw, rh = gh * ch; bool clash = false;
			for (auto& o : frags) if (o.role == Role::Island && !o.vacHoldForever) { if (rx < o.hx + o.hw && rx + rw > o.hx && ry < o.hy + o.hh && ry + rh > o.hy) clash = true; }
			if (clash) continue;
			int k = addFrag(rx, ry, rw, rh, 500 + int(frags.size()));
			Fragment& f = frags[size_t(k)]; f.role = Role::Island; f.z = 1; f.life = rng.range(7.f, 17.f);
			float r = rng.f(); f.island = r < 0.34f ? IslandKind::Zoomed : r < 0.62f ? IslandKind::Aligned : r < 0.80f ? IslandKind::Vacant : IslandKind::Effect;
			if (f.island == IslandKind::Zoomed) { f.zoom.set(rng.range(1.8f, 3.4f)); f.cx.set(rng.range(-0.18f, 0.18f)); f.cy.set(rng.range(-0.15f, 0.15f)); }
			if (f.island == IslandKind::Vacant) f.solid = true;
			f.vacHold = 0; f.vacFadeIn = rng.range(0.4f, 0.8f);
			return;
		}
	}

	// INTERRUPT: CHAOS -> SORTING -> NEAR IMAGE -> INTERRUPTION
	void stepInterruptMode(float dt) {
		float thr = dialPhys(ModeId::INTERRUPT, 0), hold = dialPhys(ModeId::INTERRUPT, 1);
		int n = int(order.size());
		ms[0] -= dt; // phase timer / event timer
		switch (mi[0]) {
			case 0: // CHAOS
				phaseName = "CHAOS";
				if (ms[0] <= 0) { // re-scramble pulses
					misassemble(rng.irange(3, 8), 2, pickCurve(), pickOp(), 0.9f); ms[0] = rng.range(0.5f, 0.9f); ms[1] += 0.7f;
					if (rng.chance(0.4f)) { Fragment& f = frags[size_t(order[size_t(rng.irange(0, n - 1))])]; go(f.cx, rng.sign() * rng.range(0.03f, 0.1f), 0.8f, pickCurve(), 0, 0.03f); }
				}
				if (ms[1] > 3.2f) { mi[0] = 1; ms[0] = 0.2f; ms[1] = 0; }
				break;
			case 1: // SORTING
				phaseName = "SORTING";
				if (ms[0] <= 0) {
					for (int k : prim) if (std::fabs(frags[size_t(k)].cx.target()) > 0.001f) go(frags[size_t(k)].cx, 0.f, 0.6f, Policy::GLIDE);
					sortStep(std::max(2, n / 6), 1.f, pickCurve(), pickOp(), rng.range(0.5f, 0.9f)); ms[0] = rng.range(0.30f, 0.55f);
				}
				if (coherenceNow() >= thr) { mi[0] = 2; ms[0] = hold; ms[1] = 0; }
				break;
			case 2: // NEAR IMAGE: dwell, tiny residual breathing, then abort
				phaseName = "NEAR IMAGE";
				if (coherenceNow() < thr - 0.02f && ms[1] <= 0) { ms[1] = 0.3f; sortStep(1, 1.f, Policy::GLIDE, Policy::SWAP, 0.5f); }
				ms[1] -= dt;
				if (ms[0] <= 0) { mi[0] = 3; ms[0] = 0.01f; }
				break;
			case 3: // INTERRUPTION
				phaseName = "INTERRUPTION";
				misassemble(rng.irange(5, 9), rng.irange(3, 5), mask() & bit(Policy::SCATTER) ? Policy::SCATTER : Policy::SNAP, pickOp(), 0.7f);
				for (int j = 0; j < 4; ++j) { Fragment& f = frags[size_t(order[size_t(rng.irange(0, n - 1))])]; go(f.cx, rng.sign() * rng.range(0.05f, 0.12f), 0.15f, Policy::SNAP); }
				vacancyBurst(2, 0.8f);
				++interrupts; mi[0] = 0; ms[0] = 0.6f; ms[1] = -1.5f; break;
		}
	}

	// ---- vacancy ------------------------------------------------------------------------------------
	void vacate(Fragment& f, float delay, float fadeOut, float hold, float fadeIn, bool forever) {
		f.vacDelay = delay; f.vacFadeOut = std::max(0.01f, fadeOut); f.vacHold = hold; f.vacFadeIn = std::max(0.01f, fadeIn);
		f.vacHoldForever = forever; f.vacPending = true;
	}
	void vacancyBurst(int count, float fadeOut) {
		for (int c = 0; c < count && !prim.empty(); ++c) { Fragment& f = frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])]; if (f.vac == Vac::Content && !f.nested) vacate(f, rng.range(0.f, 0.2f), fadeOut, rng.range(1.2f, 3.f), 0.8f, false); }
	}
	void stepVacancy(float dt) {
		for (auto& f : frags) vacStep(f, dt);
		vacTimer -= dt;
		if (vacTimer > 0 || !spec(mode_).allowVacancy || prim.empty()) return;
		vacTimer = rng.range(5.f, 15.f);
		if (rng.chance(0.38f)) { vacTimer = rng.range(10.f, 22.f); return; } // full-occupancy dwell
		std::vector<int> cand; for (int k : prim) if (frags[size_t(k)].vac == Vac::Content && !frags[size_t(k)].nested && frags[size_t(k)].role == Role::Base) cand.push_back(k);
		if (cand.empty()) return;
		std::sort(cand.begin(), cand.end(), [&](int a, int b) { return frags[size_t(a)].hx + frags[size_t(a)].hy * 0.01f < frags[size_t(b)].hx + frags[size_t(b)].hy * 0.01f; });
		int n = int(cand.size()); float limit = 0.30f * W * H; float used = 0;
		auto take = [&](int ci, float delay, float hold) { if (ci < 0 || ci >= n) return; Fragment& f = frags[size_t(cand[size_t(ci)])]; float a = f.hw * f.hh; if (used + a > limit || f.vacPending) return; used += a; vacate(f, delay, rng.range(0.4f, 0.9f), hold, rng.range(0.6f, 1.2f), false); };
		int kind = rng.irange(0, 5);
		switch (kind) {
			case 0: for (int j = 0; j < rng.irange(1, 3); ++j) take(rng.irange(0, n - 1), rng.range(0.f, 1.f), rng.range(2.f, 6.f)); break; // isolated
			case 1: { int N = rng.irange(3, 5), o = rng.irange(0, N - 1); for (int i = o; i < n; i += N) take(i, rng.range(0.f, 0.5f), rng.range(3.f, 6.f)); break; } // every Nth
			case 2: for (int i = 0; i < n; ++i) if (i % 2 == 0 || rng.chance(0.3f)) take(i, float(i) / float(n) * 1.6f, rng.range(0.9f, 2.2f)); break; // wave
			case 3: { int c = rng.irange(0, n - 1); for (int i = 0; i < n; ++i) take(i, std::abs(i - c) * 0.22f, 4.f); break; } // expanding
			case 4: { int c = rng.irange(0, n - 1); for (int j = 0; j < 8; ++j) { take(c + j, j * 0.5f, 0.9f); take(c + j + 1, j * 0.5f, 0.9f); } break; } // moving window
			case 5: { int c = rng.irange(0, n - 1), k = rng.irange(2, 4); for (int i = c; i < c + k; ++i) take(i, 0.f, rng.range(3.f, 6.f)); break; } // contiguous
		}
	}
	void vacStep(Fragment& f, float dt) {
		if (f.vacPending && f.vac == Vac::Content) { f.vacDelay -= dt; if (f.vacDelay <= 0) { f.vac = Vac::FadingOut; f.vacT = 0; f.vacPending = false; } }
		else if (f.vacPending && f.vac == Vac::FadingIn) { f.vacDelay -= dt; if (f.vacDelay <= 0) { f.vac = Vac::FadingOut; f.vacT = f.vacFadeOut * (1.f - f.alpha); f.vacPending = false; } }
		else if (f.vacPending && f.vac == Vac::Vacant && !f.vacHoldForever) { f.vacPending = false; }
		switch (f.vac) {
			case Vac::Content: f.alpha = 1.f; break;
			case Vac::FadingOut:
				f.vacT += dt; f.alpha = 1.f - smooth01(f.vacT / f.vacFadeOut);
				if (f.vacT >= f.vacFadeOut) { f.vac = Vac::Vacant; f.vacT = 0; f.alpha = 0; releaseEffect(f);
					if (f.pendHas) { f.flipX = f.pendFlip; f.cx.set(f.pendCx); f.cy.set(f.pendCy); f.pendHas = false; } }
				break;
			case Vac::Vacant:
				f.alpha = 0; f.vacT += dt; releaseEffect(f);
				if (!f.vacHoldForever && f.vacT >= f.vacHold) { f.vac = Vac::FadingIn; f.vacT = 0; }
				break;
			case Vac::FadingIn:
				f.vacT += dt; f.alpha = smooth01(f.vacT / f.vacFadeIn);
				if (f.vacT >= f.vacFadeIn) { f.vac = Vac::Content; f.alpha = 1.f; }
				break;
		}
	}
	void releaseEffect(Fragment& f) { if (f.effectSlot >= 0) { f.effectSlot = -1; f.effAlpha = 0; f.effTarget = 0; } }

	// ---- zoom director (independent source magnification) ----------------------------------------------
	void stepZoom(float dt) {
		for (auto& f : frags) if (f.zoomHold > 0) { f.zoomHold -= dt; if (f.zoomHold <= 0 && f.role == Role::Base) go(f.zoom, 1.f, rng.range(0.8f, 1.5f), Policy::GLIDE); }
		zoomTimer -= dt;
		if (zoomTimer > 0 || !spec(mode_).allowZoom || prim.empty()) return;
		zoomTimer = rng.range(9.f, 24.f);
		if (rng.chance(0.3f)) return;
		static const float zs[] = {1.6f, 2.0f, 3.0f, 4.0f};
		int c = rng.irange(0, int(prim.size()) - 1), k = rng.irange(1, 4); float z = zs[rng.irange(0, 3)];
		for (int j = 0; j < k && c + j < int(prim.size()); ++j) {
			Fragment& f = frags[size_t(prim[size_t(c + j)])]; if (f.vac != Vac::Content || f.nested) continue;
			go(f.zoom, z, rng.range(0.8f, 1.5f), rng.chance(0.5f) ? Policy::GLIDE : Policy::OVERSHOOT, j * 0.12f); f.zoomHold = rng.range(3.5f, 8.f);
		}
	}

	// ---- final geometry --------------------------------------------------------------------------------
	void finalizeOne(Fragment& f) {
		float X = f.x.out(clock) + f.px, Y = f.y.out(clock) + f.py, Wd = std::max(0.f, f.w.v + f.pw), Hd = std::max(0.f, f.h.v + f.ph);
		float zMin = std::max(Wd / W, Hd / H), zm = std::max(zMin, std::max(0.05f, f.zoom.v + f.pzoom));
		f.cropZoom = zm;
		float su = Wd / W / zm, sv = Hd / H / zm; su = std::min(su, 1.f); sv = std::min(sv, 1.f);
		float u = f.homeU + f.cx.out(clock) + f.pcx, v = f.homeV + f.cy.out(clock) + f.pcy;
		float x0 = std::max(0.f, X), y0 = std::max(0.f, Y), x1 = std::min(W, X + Wd), y1 = std::min(H, Y + Hd);
		if (x1 <= x0 + 0.01f || y1 <= y0 + 0.01f || Wd <= 0.01f || Hd <= 0.01f) { f.dx = f.dy = f.dw = f.dh = 0; f.visible = false; f.u0 = f.u1 = f.v0 = f.v1 = 0; return; }
		float ua = u - su * 0.5f, ub = u + su * 0.5f, va = v - sv * 0.5f, vb = v + sv * 0.5f;
		float fu0 = (x0 - X) / Wd, fu1 = (x1 - X) / Wd, fv0 = (y0 - Y) / Hd, fv1 = (y1 - Y) / Hd;
		float cu0 = ua + (ub - ua) * fu0, cu1 = ua + (ub - ua) * fu1, cv0 = va + (vb - va) * fv0, cv1 = va + (vb - va) * fv1;
		clampSpan(cu0, cu1); clampSpan(cv0, cv1);
		f.dx = x0; f.dy = y0; f.dw = x1 - x0; f.dh = y1 - y0;
		f.u0 = f.flipX ? cu1 : cu0; f.u1 = f.flipX ? cu0 : cu1; f.v0 = f.flipY ? cv1 : cv0; f.v1 = f.flipY ? cv0 : cv1;
		f.visible = f.drawn && (f.alpha > 0.002f || f.solid);
		f.visible = f.visible && !(f.nested && f.alpha <= 0.002f);
	}
	void finalize() { for (auto& f : frags) finalizeOne(f); for (auto& f : ghosts) finalizeOne(f); }

	// ---- coherence + interruption ------------------------------------------------------------------------
	float fragCoherence(const Fragment& f) const {
		float X = f.x.v + f.px, Y = f.y.v + f.py, Wd = f.w.v + f.pw, Hd = f.h.v + f.ph;
		float dxe = std::fabs((X + Wd * 0.5f) - (f.hx + f.hw * 0.5f)), dye = std::fabs((Y + Hd * 0.5f) - (f.hy + f.hh * 0.5f));
		float posE = (dxe + dye) / (0.12f * W);
		float cropE = (std::fabs(f.cx.v + f.pcx) * W + std::fabs(f.cy.v + f.pcy) * H) / (0.10f * W);
		float zoomE = std::fabs(f.zoom.v + f.pzoom - 1.f) / 0.6f;
		float e = posE + cropE + zoomE + (f.flipX ? 1.f : 0.f);
		return 1.f - smooth01(e);
	}
	float coherenceNow() const {
		float num = 0;
		for (const auto& f : frags) {
			if (f.role == Role::Child || f.ghost || f.solid) continue;
			if (f.role == Role::Base && f.nested) continue;
			float a = f.alpha; if (a <= 0.f) continue;
			num += a * fragCoherence(f) * (f.hw * f.hh);
		}
		float tot = W * H; return clamp01(num / tot);
	}
	void stepCoherence(float dt) {
		float C = coherenceNow(); tel.coherence = C;
		for (auto& f : frags) f.coherence = fragCoherence(f);
		interruptCooldown -= dt;
		float thr = mode_ == ModeId::INTERRUPT ? dialPhys(ModeId::INTERRUPT, 0) : 0.88f;
		if (mode_ != ModeId::INTERRUPT) {
			if (C >= thr) nearT += dt; else nearT = std::max(0.f, nearT - dt);
			float hold = 0.9f + 1.4f * float(gen % 3);
			if (nearT >= hold && interruptCooldown <= 0.f) { interrupt(false); nearT = 0; interruptCooldown = rng.range(2.f, 4.f); }
		}
		int guard = 0; // hard ceiling: never perfect, even for an instant
		while (coherenceNow() > kCoherenceCeiling && guard++ < 12) interrupt(true);
		C = coherenceNow(); tel.coherence = C; maxCoh = std::max(maxCoh, C);
	}
	// Interruption event: displacement / crop shift / vacancy / rephase.
	void interrupt(bool hard) {
		++interrupts; if (prim.empty()) return;
		int type = rng.irange(0, 3);
		bool orderMode = !order.empty();
		if (hard) type = 1;
		if (type == 0 && !orderMode) type = 1;
		switch (type) {
			case 0: { Policy op = pickOp(), cu = pickCurve(); misassemble(rng.irange(3, 8), rng.irange(1, 3), cu, op, rng.range(0.6f, 1.2f)); break; }
			case 1: {
				int k = hard ? 1 : rng.irange(2, 4);
				for (int j = 0; j < k; ++j) {
					Fragment* fp = &frags[size_t(prim[size_t(rng.irange(0, int(prim.size()) - 1))])];
					float mag = rng.sign() * rng.range(0.06f, 0.14f);
					if (hard) { fp->cx.set(fp->cx.v + mag); fp->pcx = 0; } else go(fp->cx, fp->cx.target() + mag, rng.range(0.3f, 0.7f), pickCurve(), 0, 0.03f);
				}
				break; }
			case 2: vacancyBurst(rng.irange(1, 3), 0.5f); break;
			case 3: phaseOff.go(phaseOff.v + rng.sign() * rng.range(0.25f, 0.6f), rng.range(1.2f, 2.f), Policy::GLIDE); if (mode_ == ModeId::FAULT) { ms[0] = 0; } if (mode_ == ModeId::SCAN) { ms[2] += 1.f; } break;
		}
	}

	// ---- ghosts (outgoing layout) --------------------------------------------------------------------------
	void stepGhosts(float dt) {
		for (auto& f : ghosts) { f.alpha = std::max(0.f, f.alpha - dt / 0.6f); if (f.effectSlot >= 0) { f.effAlpha = std::max(0.f, f.effAlpha - dt * 1.6f); if (f.effAlpha <= 0) { f.effectSlot = -1; } } }
		ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(), [](const Fragment& f) { return f.alpha <= 0.001f; }), ghosts.end());
	}

	// ---- effect director ------------------------------------------------------------------------------------
	static float rectArea(const Fragment& f) { return f.dw * f.dh; }
	// Conservative reservation: the largest destination area the fragment may take (current or target), clipped to the canvas.
	float areaBound(const Fragment& f) const {
		float cw = std::max(f.w.v + f.pw, f.w.target()), ch = std::max(f.h.v + f.ph, f.h.target());
		cw = std::min(cw, W); ch = std::min(ch, H);
		float cur = f.dw * f.dh;
		return std::max(cur, cw * ch) * 1.12f;
	}
	int countAssigned(int s) const { int n = 0; for (const auto& f : frags) if (f.effectSlot == s) ++n; for (const auto& f : ghosts) if (f.effectSlot == s) ++n; return n; }
	float usedBound() const { float u = 0; for (const auto& f : frags) if (f.effectSlot >= 0) u += areaBound(f); for (const auto& f : ghosts) if (f.effectSlot >= 0) u += areaBound(f); return u; }
	bool eligibleForEffect(const Fragment& f) const {
		if (f.ghost || f.solid || !f.drawn || f.effectSlot >= 0) return false;
		if (f.nested && f.role == Role::Base) return false;
		if (f.vac != Vac::Content || f.alpha < 0.98f || f.vacPending) return false;
		if (f.role == Role::Child && f.parent >= 0 && frags[size_t(f.parent)].vac != Vac::Vacant) return false;
		if (f.dw * f.dh < 40.f * 40.f) return false;
		return true;
	}
	bool startSlot(int s, const std::string& id, Pattern pat, float frac) {
		Slot& sl = slots[size_t(s)]; if (!sl.id.empty() || id.empty()) return false;
		const Slot& other = slots[size_t(1 - s)]; if (other.id == id) return false;
		sl.id = id; sl.active = true; sl.retiring = false; sl.pattern = int(pat); sl.targetFrac = frac; sl.life = rng.range(14.f, 34.f); sl.assignClock = 0.f;
		sl.crest = rng.chance(0.5f) ? -0.1f : 1.1f; sl.crestDir = sl.crest < 0.f ? 1.f : -1.f;
		buildQueue(sl, pat);
		return true;
	}
	void buildQueue(Slot& sl, Pattern pat) {
		sl.queue.clear(); sl.qpos = 0;
		struct K { int i; float key; };
		std::vector<K> ks;
		std::vector<int> pool;
		for (size_t k = 0; k < frags.size(); ++k) if (frags[k].role != Role::Base || !frags[k].nested) pool.push_back(int(k));
		bool lr = rng.chance(0.5f); int seedIdx = pool.empty() ? 0 : pool[size_t(rng.irange(0, int(pool.size()) - 1))];
		int parity = rng.irange(0, 1);
		for (int i : pool) {
			const Fragment& f = frags[size_t(i)]; float cxp = f.hx + f.hw * 0.5f, cyp = f.hy + f.hh * 0.5f; float key = 0;
			switch (pat) {
				case Pattern::Sweep: key = lr ? cxp : -cxp; break;
				case Pattern::CenterOut: key = std::fabs(cxp - W * 0.5f) + std::fabs(cyp - H * 0.5f) * 0.4f; break;
				case Pattern::Infection: { const Fragment& s = frags[size_t(seedIdx)]; float dxs = cxp - (s.hx + s.hw * 0.5f), dys = cyp - (s.hy + s.hh * 0.5f); key = std::sqrt(dxs * dxs + dys * dys) + rng.range(0.f, 40.f); break; }
				case Pattern::Bands: if ((f.idx % 2) != parity) continue; key = cxp + rng.range(0.f, 20.f); break;
				case Pattern::WaveCrest: key = cxp; break;
				default: break;
			}
			if (mode_ == ModeId::BLOCK && f.role == Role::Island) key -= 1e5f; // islands first
			ks.push_back({i, key});
		}
		std::sort(ks.begin(), ks.end(), [](const K& a, const K& b) { return a.key < b.key; });
		for (auto& k : ks) sl.queue.push_back(k.i);
	}
	void retire(int s) {
		Slot& sl = slots[size_t(s)]; if (sl.id.empty() || sl.retiring) return;
		sl.retiring = true;
		auto fade = [&](Fragment& f) { if (f.effectSlot == s) { f.effTarget = 0; f.effRate = rng.range(0.4f, 1.0f); } };
		for (auto& f : frags) fade(f); for (auto& f : ghosts) fade(f);
	}
	void stepEffects(float dt) {
		// 1) fade effAlpha toward target for every fragment (entering = rising, exiting = falling)
		auto adv = [&](Fragment& f) {
			if (f.effectSlot < 0) { f.effAlpha = 0; return; }
			if (f.effLife >= 0.f) { f.effLife -= dt; if (f.effLife <= 0.f && f.effTarget > 0.f) { f.effTarget = 0.f; f.effRate = 0.6f; } }
			if (f.effAlpha < f.effTarget) f.effAlpha = std::min(f.effTarget, f.effAlpha + dt * f.effRate); else f.effAlpha = std::max(f.effTarget, f.effAlpha - dt * f.effRate);
			if (f.effTarget <= 0.f && f.effAlpha <= 0.f) { f.effectSlot = -1; f.effLife = -1.f; }
		};
		for (auto& f : frags) adv(f); for (auto& f : ghosts) adv(f);
		// 2) slot lifecycle
		for (int s = 0; s < 2; ++s) {
			Slot& sl = slots[size_t(s)];
			if (sl.id.empty()) {
				sl.idleT -= dt;
				if (sl.idleT <= 0.f) {
					bool can = effectsEnabled && !candidates.empty() && !prim.empty();
					if (can) {
						// avoid a free third shader: only one slot may be (re)started per frame, never while the other id is identical
						std::vector<std::string> opts; for (auto& c : candidates) if (c != slots[1 - s].id && c != sl.lastId) opts.push_back(c);
						if (opts.empty()) opts = candidates;
						std::string id = opts[size_t(rng.irange(0, int(opts.size()) - 1))];
						Pattern pat = Pattern(rng.irange(0, int(Pattern::Count) - 1));
						float frac = rng.range(0.04f, pi ? 0.07f : 0.115f);
						if (!startSlot(s, id, pat, frac)) sl.idleT = 2.f;
					} else sl.idleT = 4.f;
				}
				continue;
			}
			int assigned = countAssigned(s);
			if (!sl.retiring) {
				sl.life -= dt;
				if (sl.life <= 0.f) { retire(s); continue; }
				if (!effectsEnabled) { retire(s); continue; }
				sl.assignClock += dt;
				Pattern pat = Pattern(sl.pattern);
				if (pat == Pattern::WaveCrest) { sl.crest += sl.crestDir * 0.14f * dt; }
				if (sl.assignClock >= 0.12f) {
					sl.assignClock = 0.f;
					float slotArea = 0; for (const auto& f : frags) if (f.effectSlot == s) slotArea += areaBound(f);
					float budget = std::min(sl.targetFrac * W * H, kEffectPlanCap * W * H * 0.5f);
					int guard = 0;
					while (sl.qpos < sl.queue.size() && guard++ < 6) {
						int i = sl.queue[sl.qpos++]; if (i >= int(frags.size())) continue;
						Fragment& f = frags[size_t(i)];
						if (!eligibleForEffect(f)) continue;
						if (pat == Pattern::WaveCrest) { float cxn = (f.dx + f.dw * 0.5f) / W; if (std::fabs(cxn - sl.crest) > 0.10f) { if (sl.crestDir * (cxn - sl.crest) > 0) { --sl.qpos; break; } continue; } }
						float need = areaBound(f);
						if (slotArea + need > budget) continue;
						if (usedBound() + need > kEffectPlanCap * W * H) continue; // reserve BEFORE the fragment starts entering
						f.effectSlot = s; f.effAlpha = 0.f; f.effTarget = 1.f; f.effRate = rng.range(0.7f, 1.4f);
						f.effLife = pat == Pattern::WaveCrest ? rng.range(2.2f, 3.6f) : (mode_ == ModeId::BLOCK && f.role == Role::Island ? -1.f : rng.range(9.f, 28.f));
						slotArea += need; break;
					}
					if (sl.qpos >= sl.queue.size() && pat != Pattern::WaveCrest && assigned == 0) { buildQueue(sl, pat); }
				}
			} else if (assigned == 0) { // fully faded out: the canonical id leaves; the slot is free for a new combination later
				sl.lastId = sl.id; sl.id.clear(); sl.active = false; sl.retiring = false; sl.idleT = rng.range(4.f, 22.f); sl.queue.clear(); sl.qpos = 0;
			}
		}
		// 3) a slot whose fragments have all vanished without retiring (mode switch / vacancy) also frees up
		for (int s = 0; s < 2; ++s) { Slot& sl = slots[size_t(s)]; if (!sl.id.empty() && !sl.retiring && sl.qpos > 0 && countAssigned(s) == 0 && sl.assignClock > 99.f) retire(s); }
	}
	// Final safety net: exact clipped destination areas of the geometry that is about to be drawn.
	void enforceEffectArea() {
		const float cap = kEffectAreaCap * W * H * 0.985f;
		float sum = 0;
		for (const auto& f : frags) if (f.effectSlot >= 0 && f.visible) sum += rectArea(f);
		for (const auto& f : ghosts) if (f.effectSlot >= 0 && f.visible) sum += rectArea(f);
		if (sum <= cap) return;
		std::vector<Fragment*> hold;
		for (auto& f : frags) if (f.effectSlot >= 0 && f.visible) hold.push_back(&f);
		for (auto& f : ghosts) if (f.effectSlot >= 0 && f.visible) hold.push_back(&f);
		std::sort(hold.begin(), hold.end(), [](const Fragment* a, const Fragment* b) { return a->effAlpha < b->effAlpha; });
		for (Fragment* f : hold) { if (sum <= cap) break; sum -= rectArea(*f); f->effectSlot = -1; f->effAlpha = 0; f->effTarget = 0; ++forced; }
	}

	// ---- telemetry --------------------------------------------------------------------------------------------
	void computeTelemetry() {
		float effArea = 0, visible = 0; int act = 0; bool used[2] = {false, false};
		auto acc_ = [&](const Fragment& f) {
			if (!f.visible) return;
			++act; if (f.solid) return;
			visible += rectArea(f) * clamp01(f.alpha);
			if (f.effectSlot >= 0) { effArea += rectArea(f); used[f.effectSlot] = true; }
		};
		for (const auto& f : frags) if (f.role != Role::Island && !(f.role == Role::Base && f.nested && f.alpha <= 0.002f)) acc_(f); else if (f.role == Role::Island) acc_(f);
		for (const auto& f : ghosts) { if (f.visible) { ++act; if (f.effectSlot >= 0) { effArea += rectArea(f); used[f.effectSlot] = true; } } }
		float tot = W * H;
		tel.effectAreaFrac = effArea / tot; maxEff = std::max(maxEff, tel.effectAreaFrac); tel.maxEffectAreaFrac = maxEff;
		int dist = 0; std::string ids[2];
		for (int s = 0; s < 2; ++s) if (used[s]) { ids[s] = slots[size_t(s)].id; }
		if (used[0]) ++dist; if (used[1] && !(used[0] && ids[0] == ids[1])) ++dist;
		tel.effectCount = dist;
		float cover = std::min(1.f, visible / tot);
		tel.blankFrac = 1.f - cover; tel.desaturatedFrac = std::max(0.f, cover - tel.effectAreaFrac);
		tel.activeFragments = act; tel.maxCoherence = maxCoh; tel.interrupts = interrupts; tel.forcedEffectReleases = forced;
		tel.phase = phaseName; tel.mode = mode_;
	}

	// ---- auto cycle ---------------------------------------------------------------------------------------------
	void autoCycleStep(float dt) {
		if (!autoCycle) return;
		cycleTimer -= dt; if (cycleTimer > 0) return;
		if (bag.empty()) { for (int i = 0; i < kModeCount; ++i) bag.push_back(i); for (int i = kModeCount - 1; i > 0; --i) std::swap(bag[size_t(i)], bag[size_t(rng.irange(0, i))]); if (bag.back() == int(mode_)) std::swap(bag.front(), bag.back()); }
		int next = bag.back(); bag.pop_back();
		if (next == int(mode_)) { cycleTimer = 1.f; return; }
		rebuild(ModeId(next)); cycleTimer = rng.range(50.f, 85.f);
	}
};

// Bounded-effect-ID helper used by the scene to enforce eligibility semantics in tests: a candidate is
// positively classified only if every flag is explicitly good. `piSafe` must be known-true on Pi.
inline bool effectEligible(bool singlePass, bool safeForAuto, int piSafeState /*-1 unknown, 0 false, 1 true*/, bool piProfile) {
	if (!singlePass || !safeForAuto) return false;
	if (piSafeState == 0) return false;
	if (piProfile && piSafeState != 1) return false; // unclassified != eligible on Pi
	return true;
}

} // namespace shred
