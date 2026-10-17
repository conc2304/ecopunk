// blob_preset_coverage — predeploy Blob detection preset coverage check.
//
//   blob_preset_coverage [--media-root DIR] [--profiles FILE] [--catalog FILE]
//
// Defaults are the canonical repository paths (relative to the repo root,
// resolved from this binary's own location):
//   assets/shared/media
//   assets/shared/blob/blob-detection-profiles.json
//   no catalog overrides (what ExperienceRuntime uses today)
//
// Exit status: 0 PASS, 1 WARNING (missing presets), 2 ERROR (invalid
// inputs). The last line is always "Result: PASS|WARNING|ERROR". Prompting
// and overrides live in scripts/check-blob-detection-presets.sh.

#include "BlobPresetCoverage.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char ** argv) {
	namespace fs = std::filesystem;
	// tools/blob-preset-coverage/<binary> -> repo root.
	fs::path repoRoot = fs::absolute(fs::path(argv[0])).parent_path().parent_path().parent_path();
	std::string mediaRoot = (repoRoot / "assets/shared/media").string();
	std::string profiles = (repoRoot / "assets/shared/blob/blob-detection-profiles.json").string();
	std::string catalog;

	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto value = [&](std::string & out) {
			if (i + 1 >= argc) {
				std::cerr << "missing value for " << a << "\n";
				std::exit(2);
			}
			out = argv[++i];
		};
		if (a == "--media-root") {
			value(mediaRoot);
		} else if (a == "--profiles") {
			value(profiles);
		} else if (a == "--catalog") {
			value(catalog);
		} else if (a == "-h" || a == "--help") {
			std::cout << "Usage: " << argv[0] << " [--media-root DIR] [--profiles FILE] [--catalog FILE]\n"
					  << "Exit: 0 PASS, 1 WARNING (missing presets), 2 ERROR (invalid inputs)\n";
			return 0;
		} else {
			std::cerr << "unknown argument: " << a << "\n";
			return 2;
		}
	}

	using namespace blobpresetcoverage;
	Report report = evaluate(mediaRoot, profiles, catalog);
	std::cout << format(report, profiles);
	return report.result == Result::Pass ? 0 : report.result == Result::Warn ? 1 : 2;
}
