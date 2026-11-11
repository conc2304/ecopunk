// OC-003-F -- DIAGNOSTIC palette data sink (OpenFrameworks-free, header-only). Inert unless EXPERIENCE_RUNTIME_PALETTE_DIAG=<dir> is set.
//
// Receives the grid a sampler ALREADY acquired (GPU FX / GPU RAW collect, or the CPU RAW sampler), runs palette::analyze on it, and writes
//   <dir>/palette_<stream>.csv      one row per acquired sample (bounded: kMaxRows per stream; later rows are counted, not stored)
//   <dir>/grids/<stream>_g<gen>_<n>.f32   a BOUNDED set of representative 160x90x3 float32 linear-light grids (first sample of every
//                                   generation + one every gridEverySeconds, at most gridCap files per stream) for the OFFLINE
//                                   multi-resolution comparison and the contact sheets (scripts/oc003/palette_report.py)
//   <dir>/palette_summary.json      parameters, counts, rows/grids dropped (written on close)
// No allocation per sample (fixed buffers); file I/O happens only for CSV rows (stdio buffered) and the few grid snapshots, and its time is
// returned to the caller so it is recorded separately (frame CSV column pal_us) and never hidden inside acquisition cost.
#pragma once

#include "PaletteAnalyzer.h"
#include "SceneColorSeries.h"

#include <sys/stat.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace palette {

enum Stream : int { kFx = 0, kRawGpu = 1, kRawCpu = 2, kStreamCount = 3 };
inline const char* streamName(int s) { return s == kFx ? "fx" : s == kRawGpu ? "raw_gpu" : "raw_cpu"; }

class Sink {
public:
	static constexpr int kGridW = 160, kGridH = 90;
	static constexpr int kMaxRows = 7200; // 1 h at 2 Hz per stream

	~Sink() { close(); }

	bool open(const std::string& dir, int gridCap = 48, double gridEverySeconds = 10.0) {
		if (open_) return true;
		dir_ = dir;
		gridCap_ = gridCap < 0 ? 0 : gridCap;
		gridEvery_ = gridEverySeconds;
		if (::mkdir(dir_.c_str(), 0755) != 0 && !isDir(dir_)) return false;
		if (::mkdir((dir_ + "/grids").c_str(), 0755) != 0 && !isDir(dir_ + "/grids")) return false;
		open_ = true;
		return true;
	}
	bool isOpen() const { return open_; }

	// A gap / invalid sample: the temporal state of that stream restarts (drift is never computed across it).
	void gap(int stream) { if (stream >= 0 && stream < kStreamCount) st_[stream].havePrev = false; }

	// Returns the wall time spent in this call (analysis + file I/O), microseconds; 0 when closed or the stream is out of range.
	double add(int stream, uint64_t generation, double t, const float* grid /* kGridW*kGridH*4 floats, linear light */, const scenecolorsampling::Sample& s) {
		if (!open_ || stream < 0 || stream >= kStreamCount || grid == nullptr) return 0.0;
		const auto w0 = std::chrono::steady_clock::now();
		Per& p = st_[stream];
		if (!p.csv) openCsv(stream, p);
		if (p.havePrev && p.prevGen != generation) p.havePrev = false; // never across a generation
		const std::size_t n = static_cast<std::size_t>(kGridW) * kGridH;
		const Metrics m = analyze(grid, n, 4);
		Drift d;
		if (m.valid && p.havePrev) d = drift(p.prev, m);
		const bool newGen = !p.haveGen || p.lastGen != generation;
		std::string gridFile;
		if (m.valid && p.gridsWritten < gridCap_ && (newGen || t - p.lastGridT >= gridEvery_)) {
			char name[160];
			std::snprintf(name, sizeof(name), "grids/%s_g%llu_%03d.f32", streamName(stream), static_cast<unsigned long long>(generation), p.gridsWritten);
			if (writeGrid(dir_ + "/" + name, grid)) { gridFile = name; ++p.gridsWritten; p.lastGridT = t; }
		} else if (m.valid && p.gridsWritten >= gridCap_ && (newGen || t - p.lastGridT >= gridEvery_)) {
			++p.gridsDropped;
		}
		p.haveGen = true; p.lastGen = generation;
		if (p.rows < kMaxRows && p.csv) {
			std::fprintf(p.csv, "%s,%llu,%.3f,%d,%d,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%.5f,%.5f,",
				streamName(stream), static_cast<unsigned long long>(generation), t, p.rows, m.valid ? 1 : 0,
				s.r, s.g, s.b, s.domValid ? 1 : 0, s.domR, s.domG, s.domB, s.vibValid ? 1 : 0, s.vibR, s.vibG, s.vibB,
				m.chromaticFraction, m.chromaticEnergy, m.complexity, m.familiesActive, m.highlightOcc, m.shadowOcc);
			num(p.csv, d.hue); std::fputc(',', p.csv); num(p.csv, d.avg);
			for (int f = 0; f < kFamilies; ++f) std::fprintf(p.csv, ",%.5f", m.hueOccupancy[static_cast<std::size_t>(f)]);
			const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - w0).count();
			std::fprintf(p.csv, ",%.1f,%s\n", us, gridFile.c_str());
			if ((p.rows & 7) == 7) std::fflush(p.csv);
			++p.rows;
		} else {
			++p.rowsDropped;
		}
		p.rowsSeen++;
		if (m.valid) { p.prev = m; p.havePrev = true; p.prevGen = generation; } else p.havePrev = false;
		return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - w0).count();
	}

	void close() {
		if (!open_) return;
		open_ = false;
		for (int i = 0; i < kStreamCount; ++i) if (st_[i].csv) { std::fclose(st_[i].csv); st_[i].csv = nullptr; }
		const std::string path = dir_ + "/palette_summary.json";
		if (FILE* f = std::fopen(path.c_str(), "w")) {
			std::fprintf(f, "{\n  \"definitions\": {\"chromaMin\": %.3f, \"highlightY\": %.3f, \"shadowY\": %.3f, \"familyShare\": %.3f, \"families\": %d, \"colourSpace\": \"sRGB-encoded from linear grid\"},\n",
				kChromaMin, kHighlightY, kShadowY, kFamilyShare, kFamilies);
			std::fprintf(f, "  \"grid\": {\"w\": %d, \"h\": %d, \"format\": \"float32 RGB linear light, row-major, row 0 = top of the source\"},\n  \"streams\": {", kGridW, kGridH);
			bool first = true;
			for (int i = 0; i < kStreamCount; ++i) {
				if (!st_[i].rowsSeen) continue;
				std::fprintf(f, "%s\n    \"%s\": {\"samples\": %llu, \"rowsStored\": %d, \"rowsDropped\": %llu, \"gridsWritten\": %d, \"gridsDropped\": %llu}",
					first ? "" : ",", streamName(i), static_cast<unsigned long long>(st_[i].rowsSeen), st_[i].rows,
					static_cast<unsigned long long>(st_[i].rowsDropped), st_[i].gridsWritten, static_cast<unsigned long long>(st_[i].gridsDropped));
				first = false;
			}
			std::fprintf(f, "\n  }\n}\n");
			std::fclose(f);
		}
	}

private:
	struct Per {
		FILE* csv = nullptr;
		int rows = 0, gridsWritten = 0;
		uint64_t rowsSeen = 0, rowsDropped = 0, gridsDropped = 0;
		bool havePrev = false, haveGen = false;
		uint64_t prevGen = 0, lastGen = 0;
		double lastGridT = -1e9;
		Metrics prev;
	};
	static void num(FILE* f, float v) { if (std::isnan(v)) std::fputs("nan", f); else std::fprintf(f, "%.5f", v); }
	static bool isDir(const std::string& p) { struct stat sb; return ::stat(p.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode); }
	void openCsv(int stream, Per& p) {
		const std::string path = dir_ + "/palette_" + streamName(stream) + ".csv";
		p.csv = std::fopen(path.c_str(), "w");
		if (!p.csv) return;
		std::fputs("stream,gen,t,idx,metrics_valid,avgR,avgG,avgB,domValid,domR,domG,domB,vibValid,vibR,vibG,vibB,"
			"chromaticFraction,chromaticEnergy,complexity,familiesActive,highlightOcc,shadowOcc,driftHue,driftAvg", p.csv);
		for (int f = 0; f < kFamilies; ++f) std::fprintf(p.csv, ",occ%02d", f * 30);
		std::fputs(",palette_us,grid_file\n", p.csv);
	}
	bool writeGrid(const std::string& path, const float* grid) {
		FILE* f = std::fopen(path.c_str(), "wb");
		if (!f) return false;
		std::array<float, static_cast<std::size_t>(kGridW) * 3u> row{};
		bool ok = true;
		for (int y = 0; y < kGridH && ok; ++y) {
			for (int x = 0; x < kGridW; ++x) for (int c = 0; c < 3; ++c) row[static_cast<std::size_t>(x) * 3u + static_cast<std::size_t>(c)] = grid[(static_cast<std::size_t>(y) * kGridW + static_cast<std::size_t>(x)) * 4u + static_cast<std::size_t>(c)];
			ok = std::fwrite(row.data(), sizeof(float), row.size(), f) == row.size();
		}
		std::fclose(f);
		return ok;
	}

	bool open_ = false;
	std::string dir_;
	int gridCap_ = 48;
	double gridEvery_ = 10.0;
	std::array<Per, kStreamCount> st_{};
};

} // namespace palette
