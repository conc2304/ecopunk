// OC-003-F -- DIAGNOSTIC CPU RAW acquisition (OpenFrameworks-free, header-only, unit tested: test/cpu_raw_acquisition_tests.cpp).
//
// Reduces the decoder's EXISTING CPU-resident frame (VideoPlaybackService::currentPixels(), a borrowed const ofPixels* -- see the decoder
// feasibility report) to the same 160x90 LINEAR-LIGHT grid the GPU RAW sampler reads back, with the SAME block arithmetic as the GPU
// reduction shader (SceneColorSampler.cpp reduceFragment: start = floor(o*block), stop = min(src, max(start+1, floor((o+1)*block+0.5))),
// at most 24 texels per axis, each texel composited over black then sRGB-EOTF'd BEFORE averaging). AVG / DOM / VIB are then computed from
// that grid by the UNCHANGED production code (scenecolorsampling average, predominant::Tracker), so CPU RAW and GPU RAW are comparable
// cell by cell. Nothing here touches the GPU, the decoder or the HUD, and a CPU RAW sample is never published to a snapshot.
//
// Lifetime contract (the reason this runs synchronously on the main thread): the borrowed pixels are valid only between the
// ofVideoPlayer::update() that produced them and the next one (GStreamer: ofGstVideoUtils::update swaps front/back pixels there and drops
// the previous GstBuffer; copyPixels==false means the ofPixels point INTO that buffer). The view is therefore consumed inside one call and
// no pointer is retained.
#pragma once

#include "PredominantColor.h"
#include "RawVideoGate.h"
#include "SceneColorSeries.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace cpuraw {

constexpr int kGridW = 160, kGridH = 90;
constexpr int kMaxBlock = 24; // the GPU shader's per-axis loop cap

struct PixelView {
	const uint8_t* data = nullptr;
	int w = 0, h = 0, channels = 0; // 3 (RGB) or 4 (RGBA), 8 bit
	std::size_t rowBytes = 0;       // >= w * channels
	bool usable() const { return data != nullptr && w > 0 && h > 0 && (channels == 3 || channels == 4) && rowBytes >= static_cast<std::size_t>(w) * static_cast<std::size_t>(channels); }
};

inline double wallMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
// CPU EXECUTION time of the calling thread (not wall time): a sleeping/blocked thread does not accrue it. -1 when unavailable.
inline double threadCpuMs() {
#if defined(CLOCK_THREAD_CPUTIME_ID)
	timespec ts{};
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
#endif
	return -1.0;
}

// Fixed-capacity distribution (latest 256 values + cumulative count/sum/max): bounded, allocation-free.
class Dist {
public:
	void add(double v) {
		ring_[head_] = static_cast<float>(v);
		head_ = (head_ + 1) % ring_.size();
		if (count_ < ring_.size()) ++count_;
		++total_; sum_ += v; if (v > max_) max_ = v;
	}
	double pct(double p) const { // nearest rank over the ring
		if (count_ == 0) return 0.0;
		std::array<float, 256> tmp = ring_;
		std::sort(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(count_));
		std::size_t i = static_cast<std::size_t>(p * static_cast<double>(count_));
		if (i >= count_) i = count_ - 1;
		return tmp[i];
	}
	uint64_t total() const { return total_; }
	double mean() const { return total_ ? sum_ / static_cast<double>(total_) : 0.0; }
	double max() const { return max_; }
private:
	std::array<float, 256> ring_{};
	std::size_t head_ = 0, count_ = 0;
	uint64_t total_ = 0;
	double sum_ = 0.0, max_ = 0.0;
};

// out: kGridW*kGridH*4 floats, RGB linear light, A = 1. Returns false (out untouched) for an unusable view.
inline bool reduceToGrid(const PixelView& v, float* out) {
	if (!v.usable() || out == nullptr) return false;
	const auto& lut = scenecolorsampling::srgbLut();
	const float bx = static_cast<float>(v.w) / static_cast<float>(kGridW), by = static_cast<float>(v.h) / static_cast<float>(kGridH);
	for (int oy = 0; oy < kGridH; ++oy) {
		const int y0 = static_cast<int>(std::floor(static_cast<float>(oy) * by));
		int y1 = static_cast<int>(std::floor(static_cast<float>(oy + 1) * by + 0.5f));
		y1 = std::min(v.h, std::max(y0 + 1, y1));
		y1 = std::min(y1, y0 + kMaxBlock);
		for (int ox = 0; ox < kGridW; ++ox) {
			const int x0 = static_cast<int>(std::floor(static_cast<float>(ox) * bx));
			int x1 = static_cast<int>(std::floor(static_cast<float>(ox + 1) * bx + 0.5f));
			x1 = std::min(v.w, std::max(x0 + 1, x1));
			x1 = std::min(x1, x0 + kMaxBlock);
			float r = 0.0f, g = 0.0f, b = 0.0f;
			for (int y = y0; y < y1; ++y) {
				const uint8_t* row = v.data + static_cast<std::size_t>(y) * v.rowBytes + static_cast<std::size_t>(x0) * static_cast<std::size_t>(v.channels);
				if (v.channels == 3) {
					for (int x = x0; x < x1; ++x, row += 3) { r += lut[row[0]]; g += lut[row[1]]; b += lut[row[2]]; }
				} else {
					for (int x = x0; x < x1; ++x, row += 4) {
						if (row[3] == 255) { r += lut[row[0]]; g += lut[row[1]]; b += lut[row[2]]; }
						else { // composite over black in the ENCODED domain, then EOTF -- as the shader does
							const float a = static_cast<float>(row[3]) / 255.0f;
							r += scenecolorsampling::srgbToLinear(static_cast<float>(row[0]) / 255.0f * a);
							g += scenecolorsampling::srgbToLinear(static_cast<float>(row[1]) / 255.0f * a);
							b += scenecolorsampling::srgbToLinear(static_cast<float>(row[2]) / 255.0f * a);
						}
					}
				}
			}
			const float n = static_cast<float>((y1 - y0) * (x1 - x0));
			float* o = out + (static_cast<std::size_t>(oy) * kGridW + static_cast<std::size_t>(ox)) * 4u;
			o[0] = r / n; o[1] = g / n; o[2] = b / n; o[3] = 1.0f;
		}
	}
	return true;
}

struct Stats {
	uint64_t due = 0;            // sampling periods that came due while the gate was Eligible
	uint64_t samples = 0;        // valid samples pushed
	uint64_t gaps = 0;           // explicit invalid samples (no usable pixels / dimension mismatch / non-finite)
	uint64_t noPixels = 0;       // currentPixels() returned null while Eligible
	uint64_t badFormat = 0;      // pixels present but not packed 8-bit RGB/RGBA (e.g. BGR, planar): refused, never reinterpreted
	uint64_t dimMismatch = 0;    // pixel dimensions != source size
	uint64_t droppedGeneration = 0;
	uint64_t domValid = 0, vibValid = 0, vibInvalid = 0;
	Dist accessWall, accessCpu;  // obtaining the borrowed pixels (GStreamer: a reference; macOS AVFoundation: lazy conversion happens HERE)
	Dist reduceWall, reduceCpu;  // 960x540 -> 160x90 linear-light block average
	Dist analyzeWall, analyzeCpu; // production AVG average + predominant::Tracker step on the grid
	Dist totalWall, totalCpu;     // access + reduce + analyze
};

class Sampler {
public:
	enum class Outcome { NotDue, Sampled, Gap };

	void setPredominant(bool on) { predominant_ = on; }
	bool predominant() const { return predominant_; }
	void beginGeneration(uint64_t generation, double now) {
		series_.beginGeneration(generation, now);
		tracker_.reset(); // DOM/VIB history never crosses a generation (same rule as the GPU samplers)
		haveGrid_ = false;
	}
	bool due(double now) const { return series_.due(now); }

	// Consume ONE borrowed frame. `view` is read synchronously and not retained. accessWall/accessCpu: what the caller spent obtaining it.
	Outcome onFrame(const PixelView& view, int expectW, int expectH, double now, double accessWallMs, double accessCpuMs) {
		if (!series_.due(now)) return Outcome::NotDue;
		series_.markIssued(now);
		++stats_.due;
		const double w0 = wallMs(), c0 = threadCpuMs();
		scenecolorsampling::Sample s;
		s.t = now;
		s.generation = series_.generation();
		haveGrid_ = false;
		bool ok = false;
		if (!view.usable()) {
			if (view.data == nullptr) ++stats_.noPixels; else ++stats_.badFormat;
		} else if (view.w != expectW || view.h != expectH) {
			++stats_.dimMismatch;
		} else {
			ok = reduceToGrid(view, grid_.data());
		}
		const double w1 = wallMs(), c1 = threadCpuMs();
		if (ok) {
			double r = 0, g = 0, b = 0;
			const std::size_t n = static_cast<std::size_t>(kGridW) * kGridH;
			for (std::size_t i = 0; i < n; ++i) { r += grid_[i * 4]; g += grid_[i * 4 + 1]; b += grid_[i * 4 + 2]; }
			s.r = static_cast<float>(r / n); s.g = static_cast<float>(g / n); s.b = static_cast<float>(b / n);
			s.luma = scenecolorsampling::luma709(s.r, s.g, s.b);
			s.valid = std::isfinite(s.r) && std::isfinite(s.g) && std::isfinite(s.b) && std::isfinite(s.luma);
			if (s.valid && predominant_) {
				const predominant::Result pr = tracker_.step(grid_.data(), n, 4);
				s.domValid = pr.dom.valid; s.vibValid = pr.vib.valid;
				if (pr.dom.valid) { s.domR = pr.dom.rgb.r; s.domG = pr.dom.rgb.g; s.domB = pr.dom.rgb.b; ++stats_.domValid; }
				if (pr.vib.valid) { s.vibR = pr.vib.rgb.r; s.vibG = pr.vib.rgb.g; s.vibB = pr.vib.rgb.b; ++stats_.vibValid; }
				else ++stats_.vibInvalid;
			}
		}
		if (!s.valid) tracker_.reset(); // smoothing never bridges a gap
		const double w2 = wallMs(), c2 = threadCpuMs();
		if (!series_.push(s)) ++stats_.droppedGeneration;
		haveGrid_ = s.valid;
		last_ = s;
		if (s.valid) ++stats_.samples; else ++stats_.gaps;
		stats_.accessWall.add(accessWallMs); if (accessCpuMs >= 0) stats_.accessCpu.add(accessCpuMs);
		stats_.reduceWall.add(w1 - w0); if (c0 >= 0 && c1 >= 0) stats_.reduceCpu.add(c1 - c0);
		stats_.analyzeWall.add(w2 - w1); if (c1 >= 0 && c2 >= 0) stats_.analyzeCpu.add(c2 - c1);
		stats_.totalWall.add(accessWallMs + (w2 - w0));
		if (accessCpuMs >= 0 && c0 >= 0 && c2 >= 0) stats_.totalCpu.add(accessCpuMs + (c2 - c0));
		lastTotalWallMs_ = accessWallMs + (w2 - w0);
		return s.valid ? Outcome::Sampled : Outcome::Gap;
	}

	// Records an explicit gap when a sample is due but the source cannot be measured now (decoder stalled / dimensions changed).
	Outcome recordGap(double now) {
		if (!series_.due(now)) return Outcome::NotDue;
		series_.markIssued(now);
		scenecolorsampling::Sample s;
		s.t = now; s.generation = series_.generation(); s.valid = false;
		tracker_.reset();
		if (!series_.push(s)) ++stats_.droppedGeneration;
		haveGrid_ = false; last_ = s; ++stats_.gaps; ++stats_.due;
		return Outcome::Gap;
	}

	const Stats& stats() const { return stats_; }
	const scenecolorsampling::Series& series() const { return series_; }
	const scenecolorsampling::Sample& last() const { return last_; }
	bool haveGrid() const { return haveGrid_; }       // true only right after a Sampled outcome
	const float* grid() const { return grid_.data(); } // kGridW*kGridH*4 floats, valid iff haveGrid()
	double lastTotalWallMs() const { return lastTotalWallMs_; }

private:
	scenecolorsampling::Series series_;
	predominant::Tracker tracker_;
	std::array<float, static_cast<std::size_t>(kGridW) * kGridH * 4> grid_{};
	scenecolorsampling::Sample last_;
	Stats stats_;
	bool predominant_ = false;
	bool haveGrid_ = false;
	double lastTotalWallMs_ = 0.0;
};

} // namespace cpuraw
