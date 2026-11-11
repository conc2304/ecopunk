// OC-003-F -- PaletteSink.h (OF-free): inert when closed, bounded rows/grids, generation isolation, gap handling, determinism of the
// written CSV for identical input, and the summary file.
#include "PaletteSink.h"

#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace palette;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}

std::vector<float> grid(uint32_t seed) {
	std::vector<float> g(static_cast<std::size_t>(Sink::kGridW) * Sink::kGridH * 4);
	for (std::size_t i = 0; i < g.size(); i += 4) {
		seed = seed * 1664525u + 1013904223u;
		g[i] = static_cast<float>(seed >> 24) / 255.0f;
		seed = seed * 1664525u + 1013904223u;
		g[i + 1] = static_cast<float>(seed >> 24) / 255.0f;
		seed = seed * 1664525u + 1013904223u;
		g[i + 2] = static_cast<float>(seed >> 24) / 255.0f;
		g[i + 3] = 1.0f;
	}
	return g;
}
std::string slurp(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
int lines(const std::string& s) { int n = 0; for (char c : s) n += c == '\n'; return n; }
bool exists(const std::string& p) { return ::access(p.c_str(), F_OK) == 0; }

std::string tmpDir(const char* tag) {
	char buf[256];
	std::snprintf(buf, sizeof(buf), "/tmp/oc003f_palette_sink_%s_%d", tag, static_cast<int>(::getpid()));
	return buf;
}
void rmrf(const std::string& d) { std::string c = "rm -rf '" + d + "'"; if (std::system(c.c_str()) != 0) {} }

scenecolorsampling::Sample sample(double t, uint64_t gen) { scenecolorsampling::Sample s; s.t = t; s.generation = gen; s.valid = true; s.r = 0.2f; s.g = 0.3f; s.b = 0.4f; return s; }
} // namespace

int main() {
	// -- closed sink is inert: no files, zero cost, no crash ---------------------------------
	{
		Sink s;
		const auto g = grid(1);
		check(s.add(kRawCpu, 1, 0.0, g.data(), sample(0, 1)) == 0.0, "closed sink: add() is a no-op returning 0");
		s.gap(kRawCpu);
		s.close();
		check(!s.isOpen(), "closed sink stays closed");
	}
	// -- rows are bounded by gridCap for grids, CSV has header + one row per sample ----------------------
	{
		const std::string d = tmpDir("a");
		rmrf(d);
		Sink s;
		check(s.open(d, 2, 10.0), "open creates the directory and grids/");
		const auto g1 = grid(7), g2 = grid(8);
		for (int i = 0; i < 6; ++i) s.add(kRawCpu, 1, 0.5 * i, (i & 1 ? g2 : g1).data(), sample(0.5 * i, 1));
		s.close();
		const std::string csv = slurp(d + "/palette_raw_cpu.csv");
		check(lines(csv) == 1 + 6, "CSV = header + 6 rows");
		int grids = 0;
		for (int i = 0; i < 6; ++i) { char n[96]; std::snprintf(n, sizeof(n), "%s/grids/raw_cpu_g1_%03d.f32", d.c_str(), i); grids += exists(n); }
		check(grids == 1, "only the first sample of the generation (and none within 10 s) writes a grid: bounded");
		check(exists(d + "/palette_summary.json") && slurp(d + "/palette_summary.json").find("\"raw_cpu\"") != std::string::npos, "summary names the stream that had samples");
		check(!exists(d + "/palette_fx.csv") && !exists(d + "/palette_raw_gpu.csv"), "streams without samples create no CSV");
		rmrf(d);
	}
	// -- grid cap + generation boundary --------------------------------------------------------------------
	{
		const std::string d = tmpDir("b");
		rmrf(d);
		Sink s;
		s.open(d, 1, 10.0);
		const auto g = grid(3);
		s.add(kRawGpu, 1, 0.0, g.data(), sample(0, 1));
		s.add(kRawGpu, 2, 1.0, g.data(), sample(1, 2)); // new generation: wants a grid but the cap (1) is reached
		s.close();
		const std::string sum = slurp(d + "/palette_summary.json");
		check(sum.find("\"gridsWritten\": 1") != std::string::npos && sum.find("\"gridsDropped\": 1") != std::string::npos, "cap reached: later grids are counted as dropped, not written");
		rmrf(d);
	}
	// -- drift never crosses a gap or a generation --------------------------------------------------------------
	{
		const std::string d = tmpDir("c");
		rmrf(d);
		Sink s;
		s.open(d, 0, 10.0);
		const auto g1 = grid(11), g2 = grid(12);
		s.add(kFx, 1, 0.0, g1.data(), sample(0, 1));
		s.add(kFx, 1, 0.5, g2.data(), sample(0.5, 1)); // drift defined
		s.gap(kFx);
		s.add(kFx, 1, 1.0, g1.data(), sample(1.0, 1)); // after a gap: no drift
		s.add(kFx, 2, 1.5, g2.data(), sample(1.5, 2)); // new generation: no drift
		s.close();
		std::istringstream in(slurp(d + "/palette_fx.csv"));
		std::string line;
		std::getline(in, line);
		int row = 0;
		std::vector<std::string> driftHue;
		while (std::getline(in, line)) {
			std::vector<std::string> f;
			std::stringstream ls(line);
			std::string c;
			while (std::getline(ls, c, ',')) f.push_back(c);
			// driftHue is the 23rd column (index 22): see Sink::openCsv
			driftHue.push_back(f.size() > 22 ? f[22] : "?");
			++row;
		}
		check(row == 4, "4 rows written");
		check(driftHue.size() == 4 && driftHue[0] == "nan" && driftHue[1] != "nan" && driftHue[2] == "nan" && driftHue[3] == "nan",
			"drift is nan on the first row, after a gap and across a generation; defined only between consecutive valid rows");
		rmrf(d);
	}
	// -- determinism: identical input -> identical analysis columns (palette_us / file names excluded) ---------------------
	{
		auto run = [&](const char* tag) {
			const std::string d = tmpDir(tag);
			rmrf(d);
			Sink s;
			s.open(d, 0, 10.0);
			for (int i = 0; i < 5; ++i) { const auto g = grid(100 + i); s.add(kRawCpu, 1, 0.5 * i, g.data(), sample(0.5 * i, 1)); }
			s.close();
			std::istringstream in(slurp(d + "/palette_raw_cpu.csv"));
			std::string line, out;
			while (std::getline(in, line)) { const std::size_t k = line.rfind(',', line.rfind(',') - 1); out += line.substr(0, k) + "\n"; } // drop palette_us,grid_file
			rmrf(d);
			return out;
		};
		const std::string a = run("d1"), b = run("d2");
		check(!a.empty() && a == b, "same input twice produces byte-identical analysis columns");
	}
	std::cout << (g_failures == 0 ? "ALL PASS" : "FAILURES") << " (" << (g_total - g_failures) << "/" << g_total << ")\n";
	return g_failures == 0 ? 0 : 1;
}
