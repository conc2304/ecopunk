// Tests for blobpresetcoverage::evaluate() — the predeploy Blob preset
// coverage core. Builds throwaway media roots / profile files in a temp
// directory; never touches the real assets.
//
// Build/run: make test   (from tools/blob-preset-coverage)

#include "BlobPresetCoverage.h"
#include "MediaCatalog.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace blobpresetcoverage;

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string & file, int line, const std::string & expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

#define COV_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

struct Fixture {
	fs::path root;
	fs::path media;
	fs::path profiles;

	explicit Fixture(const std::string & name) {
		root = fs::temp_directory_path() / ("blob_preset_coverage_" + name);
		fs::remove_all(root);
		media = root / "media";
		fs::create_directories(media);
		profiles = root / "blob-detection-profiles.json";
	}
	~Fixture() { fs::remove_all(root); }

	void addMedia(const std::string & relative) {
		fs::create_directories((media / relative).parent_path());
		std::ofstream(media / relative) << "x";
	}
	void writeProfiles(const std::string & text) { std::ofstream(profiles) << text; }
	void writeProfilesFor(const std::vector<std::string> & ids) {
		std::string body = "{\"schemaVersion\": 1, \"profiles\": {";
		for (size_t i = 0; i < ids.size(); i++) {
			body += (i ? "," : "") + std::string("\"") + ids[i] + "\": {\"threshold\": 30}";
		}
		writeProfiles(body + "}}");
	}
	Report run(const std::string & catalog = "") const { return evaluate(media.string(), profiles.string(), catalog); }
};

std::string id(const std::string & file) { return MediaCatalog::synthesizeMediaId(file); }

} // namespace

void test_same_media_ids_as_production() {
	// Logged by ExperienceRuntime / blob-region-prototype for this clip.
	COV_CHECK(id("Fern_Foliage.mp4") == "media.auto.3838009d");
	COV_CHECK(id("Tenerife_Shore.mp4") == "media.auto.c7ba0540");
	Fixture f("ids");
	f.addMedia("Fern_Foliage.mp4");
	f.writeProfilesFor({});
	Report r = f.run();
	COV_CHECK(r.media.size() == 1 && r.media[0].mediaId == "media.auto.3838009d");
	COV_CHECK(r.media[0].title == MediaCatalog::synthesizeFallbackDisplayTitle("Fern_Foliage.mp4"));
}

void test_all_covered_pass() {
	Fixture f("pass");
	f.addMedia("a.mp4");
	f.addMedia("b.mp4");
	f.writeProfilesFor({ id("a.mp4"), id("b.mp4") });
	Report r = f.run();
	COV_CHECK(r.result == Result::Pass);
	COV_CHECK(r.configured.size() == 2 && r.missing.empty() && r.staleProfileIds.empty());
	COV_CHECK(format(r, "p").find("Blob detection presets: 2/2 configured") != std::string::npos);
}

void test_one_missing_warn() {
	Fixture f("one_missing");
	f.addMedia("a.mp4");
	f.addMedia("Tenerife_Shore.mp4");
	f.writeProfilesFor({ id("a.mp4") });
	Report r = f.run();
	COV_CHECK(r.result == Result::Warn);
	COV_CHECK(r.missing.size() == 1);
	COV_CHECK(r.missing[0].relativePath == "Tenerife_Shore.mp4" && r.missing[0].mediaId == "media.auto.c7ba0540");
	std::string text = format(r, "p");
	COV_CHECK(text.find("WARNING: Blob detection presets missing for 1 media item") != std::string::npos);
	COV_CHECK(text.find("media.auto.c7ba0540") != std::string::npos);
	COV_CHECK(text.find("canonical Blob detector defaults") != std::string::npos);
	COV_CHECK(text.find("To configure:") != std::string::npos);
}

void test_multiple_missing_warn() {
	Fixture f("multi_missing");
	for (const char * m : { "a.mp4", "b.mp4", "c.mp4", "d.mp4" }) f.addMedia(m);
	f.writeProfilesFor({ id("b.mp4") });
	Report r = f.run();
	COV_CHECK(r.result == Result::Warn && r.missing.size() == 3 && r.configured.size() == 1);
}

void test_empty_profiles_nonempty_media_warn() {
	Fixture f("empty_profiles");
	f.addMedia("a.mp4");
	f.addMedia("b.mp4");
	f.writeProfiles("{\"schemaVersion\": 1, \"profiles\": {}}");
	Report r = f.run();
	COV_CHECK(r.result == Result::Warn && r.missing.size() == 2);
}

void test_empty_media_pass() {
	Fixture f("empty_media");
	f.writeProfilesFor({ "media.auto.00000000" });
	Report r = f.run();
	COV_CHECK(r.result == Result::Pass);
	COV_CHECK(r.media.empty() && r.staleProfileIds.size() == 1 && !r.notes.empty());
}

void test_stale_preset_is_informational() {
	Fixture f("stale");
	f.addMedia("a.mp4");
	f.writeProfilesFor({ id("a.mp4"), "media.auto.deadbeef" });
	Report r = f.run();
	COV_CHECK(r.result == Result::Pass);
	COV_CHECK(r.staleProfileIds == std::vector<std::string> { "media.auto.deadbeef" });
	COV_CHECK(format(r, "p").find("Note: 1 Blob preset has no matching current media item") != std::string::npos);
}

void test_malformed_json_error() {
	Fixture f("malformed");
	f.addMedia("a.mp4");
	f.writeProfiles("{ \"schemaVersion\": 1, \"profiles\": { broken");
	Report r = f.run();
	COV_CHECK(r.result == Result::Error && !r.errors.empty());
	COV_CHECK(format(r, "p").find("cannot be overridden") != std::string::npos);
}

void test_unsupported_schema_error() {
	Fixture f("schema");
	f.addMedia("a.mp4");
	f.writeProfiles("{\"schemaVersion\": 2, \"profiles\": {}}");
	COV_CHECK(f.run().result == Result::Error);
}

void test_invalid_entry_is_error_not_missing() {
	Fixture f("bad_entry");
	f.addMedia("a.mp4");
	f.writeProfiles("{\"schemaVersion\": 1, \"profiles\": {\"" + id("a.mp4") + "\": {\"threshold\": \"high\"}}}");
	Report r = f.run();
	COV_CHECK(r.result == Result::Error);
}

void test_missing_inputs_error() {
	Fixture f("missing_inputs");
	f.addMedia("a.mp4");
	COV_CHECK(f.run().result == Result::Error); // no profile file
	f.writeProfilesFor({});
	COV_CHECK(evaluate((f.root / "nope").string(), f.profiles.string()).result == Result::Error); // no media root
}

void test_discovery_matches_production_rules() {
	Fixture f("discovery");
	f.addMedia("keep.mp4");
	f.addMedia("UPPER.MP4"); // ofDirectory::allowExt is case-insensitive
	f.addMedia(".hidden.mp4"); // hidden: skipped
	f.addMedia("README.md");
	f.addMedia("clip.mov");
	f.addMedia("sub/nested.mp4"); // listDir is top-level only
	std::vector<std::string> found;
	std::string err;
	COV_CHECK(discoverMedia(f.media.string(), found, err));
	COV_CHECK((found == std::vector<std::string> { "UPPER.MP4", "keep.mp4" }));
}

void test_disabled_catalog_entry_not_required() {
	Fixture f("catalog");
	f.addMedia("a.mp4");
	f.addMedia("off.mp4");
	f.writeProfilesFor({ id("a.mp4") });
	fs::path catalog = f.root / "catalog.json";
	std::ofstream(catalog) << R"({"entries": [{"relativePath": "off.mp4", "enabled": false}]})";
	Report r = f.run(catalog.string());
	COV_CHECK(r.result == Result::Pass && r.media.size() == 1);
	std::ofstream(catalog) << R"({"entries": [{"relativePath": "a.mp4", "mediaId": "media.clip.hero"}]})";
	r = f.run(catalog.string());
	COV_CHECK(r.result == Result::Warn); // curated id has no preset; old synthesized one is now stale
	COV_CHECK(r.missing.size() == 2 && r.missing[0].mediaId == "media.clip.hero" && r.staleProfileIds.size() == 1);
}

int main() {
	test_same_media_ids_as_production();
	test_all_covered_pass();
	test_one_missing_warn();
	test_multiple_missing_warn();
	test_empty_profiles_nonempty_media_warn();
	test_empty_media_pass();
	test_stale_preset_is_informational();
	test_malformed_json_error();
	test_unsupported_schema_error();
	test_invalid_entry_is_error_not_missing();
	test_missing_inputs_error();
	test_discovery_matches_production_rules();
	test_disabled_catalog_entry_not_required();

	std::cout << "blob_preset_coverage_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
