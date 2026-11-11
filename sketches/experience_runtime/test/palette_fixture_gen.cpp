// OC-003-F -- SYNTHETIC fixture generator for scripts/oc003/palette_report.py (not hardware data, not runtime output).
//   palette_fixture_gen OUT_DIR [perturb]    writes raw_cpu + raw_gpu streams through the real palette::Sink; `perturb` is added to the GPU
//   copy of every cell (0 = identical grids, 0.08 = a clearly different one).
#include "PaletteSink.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
	if (argc < 2) return 2;
	const float perturb = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 0.0f;
	palette::Sink s;
	if (!s.open(argv[1], 64, 0.0)) return 1;
	uint32_t seed = 12345;
	for (int i = 0; i < 20; ++i) {
		std::vector<float> a(static_cast<std::size_t>(160) * 90 * 4), b;
		for (std::size_t k = 0; k < a.size(); k += 4) {
			for (int c = 0; c < 3; ++c) { seed = seed * 1664525u + 1013904223u; a[k + static_cast<std::size_t>(c)] = static_cast<float>(seed >> 8) / 16777216.0f * 0.8f; }
			a[k + 3] = 1.0f;
		}
		b = a;
		for (std::size_t k = 0; k < b.size(); k += 4) for (std::size_t c = 0; c < 3; ++c) b[k + c] = std::min(1.0f, b[k + c] + perturb);
		scenecolorsampling::Sample sa, sb;
		sa.t = 0.5 * i; sa.generation = 1; sa.valid = true;
		sb = sa; sb.t = 0.5 * i + 0.01; // GPU RAW lands a few ms later in the same frame window
		double ra = 0, ga = 0, ba = 0, rb = 0, gb = 0, bb = 0;
		for (std::size_t k = 0; k < a.size(); k += 4) { ra += a[k]; ga += a[k + 1]; ba += a[k + 2]; rb += b[k]; gb += b[k + 1]; bb += b[k + 2]; }
		const double n = 160.0 * 90.0;
		sa.r = static_cast<float>(ra / n); sa.g = static_cast<float>(ga / n); sa.b = static_cast<float>(ba / n);
		sb.r = static_cast<float>(rb / n); sb.g = static_cast<float>(gb / n); sb.b = static_cast<float>(bb / n);
		s.add(palette::kRawCpu, 1, sa.t, a.data(), sa);
		s.add(palette::kRawGpu, 1, sb.t, b.data(), sb);
	}
	s.close();
	return 0;
}
