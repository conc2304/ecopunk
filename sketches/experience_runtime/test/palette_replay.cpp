// OC-003-F -- offline palette replay (OF-free). Reads 160x90x3 float32 linear-light grids written by palette::Sink and prints one analysis
// line per file using the UNCHANGED production palette::analyze, so scripts/oc003/palette_report.py can (a) check the runtime's CSV rows are
// reproducible from the saved grids and (b) check determinism by running this tool twice.
//   palette_replay GRID.f32 [GRID.f32 ...]
// Output (one line per file, fixed precision, same field order as the CSV): file,valid,chromaticFraction,chromaticEnergy,complexity,
//   familiesActive,highlightOcc,shadowOcc,occ00..occ330
#include "PaletteAnalyzer.h"

#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
	constexpr int W = 160, H = 90;
	int rc = 0;
	for (int i = 1; i < argc; ++i) {
		std::vector<float> rgb(static_cast<std::size_t>(W) * H * 3);
		FILE* f = std::fopen(argv[i], "rb");
		if (!f || std::fread(rgb.data(), sizeof(float), rgb.size(), f) != rgb.size()) {
			std::fprintf(stderr, "cannot read %s (expected %d floats)\n", argv[i], W * H * 3);
			rc = 1;
			if (f) std::fclose(f);
			continue;
		}
		std::fclose(f);
		const palette::Metrics m = palette::analyze(rgb.data(), static_cast<std::size_t>(W) * H, 3);
		std::printf("%s,%d,%.5f,%.5f,%.5f,%d,%.5f,%.5f", argv[i], m.valid ? 1 : 0, m.chromaticFraction, m.chromaticEnergy, m.complexity,
			m.familiesActive, m.highlightOcc, m.shadowOcc);
		for (int k = 0; k < palette::kFamilies; ++k) std::printf(",%.5f", m.hueOccupancy[static_cast<std::size_t>(k)]);
		std::printf("\n");
	}
	return rc;
}
