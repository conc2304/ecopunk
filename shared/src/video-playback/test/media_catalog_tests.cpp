// Standalone, dependency-free tests for MediaCatalog::build() — pure
// catalog validation/synthesis logic, no filesystem access, no JSON
// parsing, no openFrameworks. Same rationale and convention as
// sketches/blob-region-prototype/test/videoregion_math_tests.cpp,
// sketches/experience_runtime/test/lifecycle_state_tests.cpp: even
// ofRectangle.h pulls in ofConstants.h -> GL/glew.h, so this test target
// links nothing but a bare compiler and ../MediaCatalog.cpp/.h +
// ../MediaMetadata.h.
//
// Covers Implement-Shared-Video-Playback-System-Agent-Prompt.md §14.1's
// catalog test list: valid catalog, missing catalog (no overrides at
// all), empty catalog (no discovered files), duplicate media IDs, missing
// media file (an override referencing a file that wasn't discovered),
// disabled media entry, one playable item, multiple playable items.
//
// Build/run: make -C test -f Makefile.tests test

#include "../MediaCatalog.h"

#include <iostream>
#include <string>

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define MC_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_empty_catalog_no_discovered_files() {
	MediaCatalogBuildResult result = MediaCatalog::build({}, {});
	MC_CHECK(result.entries.empty());
	MC_CHECK(result.errors.empty());  // zero discovered files is not itself an error
}

static void test_single_file_no_overrides_synthesizes_entry() {
	MediaCatalogBuildResult result = MediaCatalog::build({"clip_one.mp4"}, {});
	MC_CHECK(result.entries.size() == 1);
	MC_CHECK(result.errors.empty());
	const MediaMetadata& m = result.entries[0];
	MC_CHECK(m.relativePath == "clip_one.mp4");
	MC_CHECK(!m.mediaId.empty());
	MC_CHECK(!m.titleId.empty());
	MC_CHECK(m.fallbackDisplayTitle == "clip one");  // underscores -> spaces, extension stripped
	MC_CHECK(m.enabled);
	MC_CHECK(m.piSafe);
}

static void test_multiple_files_all_playable() {
	MediaCatalogBuildResult result = MediaCatalog::build(
		{"a.mp4", "b.mp4", "c.mp4"}, {});
	MC_CHECK(result.entries.size() == 3);
	MC_CHECK(result.errors.empty());
	// mediaId must be unique across all three.
	MC_CHECK(result.entries[0].mediaId != result.entries[1].mediaId);
	MC_CHECK(result.entries[1].mediaId != result.entries[2].mediaId);
	MC_CHECK(result.entries[0].mediaId != result.entries[2].mediaId);
}

static void test_synthesized_media_id_is_stable_across_calls() {
	// Same relativePath, two independent build() calls -- mediaId must be
	// identical both times ("stable ... across a checkout").
	MediaCatalogBuildResult first = MediaCatalog::build({"repeat.mp4"}, {});
	MediaCatalogBuildResult second = MediaCatalog::build({"repeat.mp4"}, {});
	MC_CHECK(first.entries.size() == 1);
	MC_CHECK(second.entries.size() == 1);
	MC_CHECK(first.entries[0].mediaId == second.entries[0].mediaId);
}

static void test_synthesized_media_id_independent_of_catalog_order() {
	MediaCatalogBuildResult a = MediaCatalog::build({"x.mp4", "y.mp4"}, {});
	MediaCatalogBuildResult b = MediaCatalog::build({"y.mp4", "x.mp4"}, {});
	// Find "y.mp4"'s mediaId in each independently-ordered result.
	std::string yIdFromA, yIdFromB;
	for (const auto& m : a.entries) if (m.relativePath == "y.mp4") yIdFromA = m.mediaId;
	for (const auto& m : b.entries) if (m.relativePath == "y.mp4") yIdFromB = m.mediaId;
	MC_CHECK(!yIdFromA.empty());
	MC_CHECK(yIdFromA == yIdFromB);
}

static void test_override_supplies_curated_fields() {
	MediaCatalogEntryOverride ov;
	ov.relativePath = "clip.mp4";
	ov.mediaId = "media.clip.curated";
	ov.titleId = "media.title.clip.curated";
	ov.fallbackDisplayTitle = "Curated Clip";
	ov.enabled = true;
	ov.piSafe = false;

	MediaCatalogBuildResult result = MediaCatalog::build({"clip.mp4"}, {ov});
	MC_CHECK(result.entries.size() == 1);
	MC_CHECK(result.entries[0].mediaId == "media.clip.curated");
	MC_CHECK(result.entries[0].titleId == "media.title.clip.curated");
	MC_CHECK(result.entries[0].fallbackDisplayTitle == "Curated Clip");
	MC_CHECK(result.entries[0].piSafe == false);
}

static void test_disabled_override_excludes_from_playable_but_keeps_entry() {
	MediaCatalogEntryOverride ov;
	ov.relativePath = "broken.mp4";
	ov.enabled = false;

	MediaCatalogBuildResult result = MediaCatalog::build({"broken.mp4", "good.mp4"}, {ov});
	MC_CHECK(result.entries.size() == 2);  // catalog still lists it -- disabled != absent
	bool foundDisabled = false;
	for (const auto& m : result.entries) {
		if (m.relativePath == "broken.mp4") {
			foundDisabled = true;
			MC_CHECK(m.enabled == false);
		}
	}
	MC_CHECK(foundDisabled);
}

static void test_duplicate_media_id_rejected_first_wins() {
	MediaCatalogEntryOverride ov1;
	ov1.relativePath = "one.mp4";
	ov1.mediaId = "media.dup";

	MediaCatalogEntryOverride ov2;
	ov2.relativePath = "two.mp4";
	ov2.mediaId = "media.dup";  // collides with ov1

	MediaCatalogBuildResult result = MediaCatalog::build({"one.mp4", "two.mp4"}, {ov1, ov2});
	MC_CHECK(result.entries.size() == 1);  // second is rejected, not silently included
	MC_CHECK(result.entries[0].relativePath == "one.mp4");
	MC_CHECK(!result.errors.empty());
}

static void test_override_referencing_undiscovered_file_reported() {
	MediaCatalogEntryOverride ov;
	ov.relativePath = "not_actually_present.mp4";

	MediaCatalogBuildResult result = MediaCatalog::build({"present.mp4"}, {ov});
	MC_CHECK(result.entries.size() == 1);  // only the discovered file gets a catalog entry
	MC_CHECK(result.entries[0].relativePath == "present.mp4");
	bool reported = false;
	for (const auto& e : result.errors) {
		if (e.find("not_actually_present.mp4") != std::string::npos) reported = true;
	}
	MC_CHECK(reported);
}

static void test_missing_catalog_file_is_not_an_error_for_build() {
	// build() itself never touches a file -- "missing catalog" (an empty
	// overrides vector, as loadCatalogOverridesFromJsonFile() would
	// produce for a nonexistent path) is just the normal
	// auto-synthesis-only path.
	MediaCatalogBuildResult result = MediaCatalog::build({"only.mp4"}, /*overrides=*/{});
	MC_CHECK(result.entries.size() == 1);
	MC_CHECK(result.errors.empty());
}

static void test_fallback_display_title_collapses_separators() {
	MC_CHECK(MediaCatalog::synthesizeFallbackDisplayTitle("a-weird--name__here.mp4") == "a weird name here");
	MC_CHECK(MediaCatalog::synthesizeFallbackDisplayTitle("nested/dir/clip.mp4") == "clip");
}

int main() {
	test_empty_catalog_no_discovered_files();
	test_single_file_no_overrides_synthesizes_entry();
	test_multiple_files_all_playable();
	test_synthesized_media_id_is_stable_across_calls();
	test_synthesized_media_id_independent_of_catalog_order();
	test_override_supplies_curated_fields();
	test_disabled_override_excludes_from_playable_but_keeps_entry();
	test_duplicate_media_id_rejected_first_wins();
	test_override_referencing_undiscovered_file_reported();
	test_missing_catalog_file_is_not_an_error_for_build();
	test_fallback_display_title_collapses_separators();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
