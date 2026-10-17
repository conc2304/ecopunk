// Standalone tests for BlobDetectionProfileStore / BlobDetectionProfileTracker
// (per-video Blob detection presets; ../../../shared/src/BlobDetectionProfileStore.h).
// No openFrameworks, no video decoding: only the header-only nlohmann/json
// that every openFrameworks project already ships in libs/json/include.
//
// Build/run: make -C test -f Makefile.tests test

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "BlobDetectionProfileStore.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define PRO_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using LS = BlobDetectionProfileStore::LoadStatus;

const char* kTwoProfiles = R"({
  "schemaVersion": 1,
  "profiles": {
    "media.auto.aaaaaaaa": { "threshold": 28, "minBlobAreaNormalized": 0.0005, "maxBlobAreaNormalized": 0.35,
                             "erodeIterations": 0, "dilateIterations": 2, "curatorNote": "keep me" },
    "media.auto.bbbbbbbb": { "threshold": 46, "maxBlobAreaNormalized": 0.6 }
  }
})";

// Stand-in for blobdetectionprofile::canonicalDefaults() (BlobDetector::Config{}).
BlobDetectionProfile defaults() {
	BlobDetectionProfile d;
	d.threshold = 40;
	d.minBlobAreaNormalized = 0.0008;
	d.maxBlobAreaNormalized = 0.35;
	d.erodeIterations = 1;
	d.dilateIterations = 2;
	return d;
}

std::string tempPath(const char* name) {
	return std::string("/tmp/blob_profile_test_") + name + ".json";
}

std::string readFile(const std::string& path) {
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void test_parse_and_lookup() {
	BlobDetectionProfileStore s;
	PRO_CHECK(s.loadFromString(kTwoProfiles) == LS::Loaded);
	PRO_CHECK(s.size() == 2);
	PRO_CHECK(s.writable());
	const BlobDetectionProfile* a = s.find("media.auto.aaaaaaaa");
	PRO_CHECK(a != nullptr && *a->threshold == 28 && *a->erodeIterations == 0 && *a->minBlobAreaNormalized == 0.0005);
	const BlobDetectionProfile* b = s.find("media.auto.bbbbbbbb");
	PRO_CHECK(b != nullptr && *b->threshold == 46 && !b->erodeIterations && *b->maxBlobAreaNormalized == 0.6);
	PRO_CHECK(s.find("media.auto.cccccccc") == nullptr);
	PRO_CHECK(s.find("") == nullptr);
}

void test_partial_entry_resolves_to_defaults() {
	BlobDetectionProfileStore s;
	s.loadFromString(kTwoProfiles);
	BlobDetectionProfile e = resolveProfile(defaults(), s.find("media.auto.bbbbbbbb"));
	PRO_CHECK(*e.threshold == 46 && *e.maxBlobAreaNormalized == 0.6);
	PRO_CHECK(*e.erodeIterations == 1 && *e.dilateIterations == 2 && *e.minBlobAreaNormalized == 0.0008);
	PRO_CHECK(resolveProfile(defaults(), nullptr) == defaults());
}

void test_round_trip_is_stable() {
	BlobDetectionProfileStore s;
	s.loadFromString(kTwoProfiles);
	std::string once = s.serialize();
	BlobDetectionProfileStore t;
	PRO_CHECK(t.loadFromString(once) == LS::Loaded);
	PRO_CHECK(t.serialize() == once); // deterministic
	PRO_CHECK(*t.find("media.auto.aaaaaaaa") == *s.find("media.auto.aaaaaaaa"));
	PRO_CHECK(once.find("\"media.auto.aaaaaaaa\"") < once.find("\"media.auto.bbbbbbbb\"")); // sorted keys
	PRO_CHECK(once.find("0.0005,") != std::string::npos); // human-readable number
}

void test_update_one_preserves_others() {
	BlobDetectionProfileStore s;
	s.loadFromString(kTwoProfiles);
	std::string err;
	BlobDetectionProfile a = *s.find("media.auto.aaaaaaaa");
	a.threshold = 31;
	PRO_CHECK(s.set("media.auto.aaaaaaaa", a, err));
	PRO_CHECK(*s.find("media.auto.aaaaaaaa")->threshold == 31);
	PRO_CHECK(*s.find("media.auto.bbbbbbbb")->threshold == 46);
	std::string out = s.serialize();
	PRO_CHECK(out.find("\"curatorNote\": \"keep me\"") != std::string::npos); // unknown field kept
	PRO_CHECK(out.find("\"threshold\": 46") != std::string::npos);
	BlobDetectionProfile c;
	c.threshold = 55;
	PRO_CHECK(s.set("media.auto.cccccccc", c, err));
	PRO_CHECK(s.size() == 3);
}

void test_set_rejects_invalid() {
	BlobDetectionProfileStore s;
	s.loadFromString(kTwoProfiles);
	std::string err;
	BlobDetectionProfile p;
	p.threshold = 10;
	PRO_CHECK(!s.set("", p, err)); // no canonical media id -> never saved
	PRO_CHECK(!err.empty());
	p.threshold = 300;
	PRO_CHECK(!s.set("media.auto.dddddddd", p, err));
	p.threshold = 30;
	p.minBlobAreaNormalized = 0.5;
	p.maxBlobAreaNormalized = 0.2;
	PRO_CHECK(!s.set("media.auto.dddddddd", p, err));
	PRO_CHECK(s.find("media.auto.dddddddd") == nullptr);
}

void test_schema_rejection_fails_safe() {
	const char* bad[] = {
		R"({"schemaVersion": 2, "profiles": {}})",
		R"({"profiles": {}})",
		R"({"schemaVersion": 1})",
		R"({"schemaVersion": 1, "profiles": []})",
		R"(not json)",
	};
	for (const char* text : bad) {
		BlobDetectionProfileStore s;
		PRO_CHECK(s.loadFromString(text) == LS::Invalid);
		PRO_CHECK(s.size() == 0);
		PRO_CHECK(!s.writable());
		PRO_CHECK(!s.messages().empty());
		std::string err;
		PRO_CHECK(!s.save(tempPath("never_written"), err));
	}
	std::remove(tempPath("never_written").c_str());
}

void test_invalid_entry_skipped_and_preserved() {
	const char* text = R"({"schemaVersion": 1, "profiles": {
		"media.auto.good": { "threshold": 33 },
		"media.auto.bad": { "threshold": "high", "erodeIterations": 1 }
	}})";
	BlobDetectionProfileStore s;
	PRO_CHECK(s.loadFromString(text) == LS::Loaded);
	PRO_CHECK(s.find("media.auto.good") != nullptr);
	PRO_CHECK(s.find("media.auto.bad") == nullptr); // that media falls back to defaults
	PRO_CHECK(s.messages().size() == 1);
	std::string err;
	BlobDetectionProfile p;
	p.threshold = 34;
	PRO_CHECK(s.set("media.auto.good", p, err));
	PRO_CHECK(s.serialize().find("\"high\"") != std::string::npos); // bad entry kept, not silently deleted
}

void test_missing_file_and_save_load() {
	std::string path = tempPath("roundtrip");
	std::remove(path.c_str());
	BlobDetectionProfileStore s;
	PRO_CHECK(s.load(path) == LS::Missing);
	PRO_CHECK(s.writable() && s.size() == 0);
	std::string err;
	BlobDetectionProfile a;
	a.threshold = 28;
	a.erodeIterations = 0;
	PRO_CHECK(s.set("media.auto.aaaaaaaa", a, err));
	PRO_CHECK(s.save(path, err));
	BlobDetectionProfileStore t;
	PRO_CHECK(t.load(path) == LS::Loaded);
	PRO_CHECK(*t.find("media.auto.aaaaaaaa") == a);
	PRO_CHECK(readFile(path) == t.serialize());

	// Corrupt file on disk: load is Invalid and save refuses to clobber it.
	{ std::ofstream out(path); out << "{ broken"; }
	BlobDetectionProfileStore u;
	PRO_CHECK(u.load(path) == LS::Invalid);
	PRO_CHECK(!u.set("media.auto.x", a, err) || !u.save(path, err));
	PRO_CHECK(readFile(path) == "{ broken");
	std::remove(path.c_str());
}

void test_tracker_applies_once_per_media_and_restores_defaults() {
	BlobDetectionProfileStore s;
	s.loadFromString(kTwoProfiles);
	BlobDetectionProfileTracker t;
	PRO_CHECK(!t.observe(std::nullopt, s, defaults()));

	auto a = t.observe(std::string("media.auto.aaaaaaaa"), s, defaults());
	PRO_CHECK(a && a->source == "per_media" && *a->effective.threshold == 28 && *a->effective.erodeIterations == 0);
	for (int i = 0; i < 100; i++) PRO_CHECK(!t.observe(std::string("media.auto.aaaaaaaa"), s, defaults())); // not per frame

	auto b = t.observe(std::string("media.auto.bbbbbbbb"), s, defaults());
	PRO_CHECK(b && b->source == "per_media" && *b->effective.threshold == 46 && *b->effective.erodeIterations == 1);

	auto c = t.observe(std::string("media.auto.unknown0"), s, defaults());
	PRO_CHECK(c && c->source == "defaults" && c->effective == defaults()); // B does not leak into C

	auto a2 = t.observe(std::string("media.auto.aaaaaaaa"), s, defaults());
	PRO_CHECK(a2 && a2->effective == a->effective);

	t.invalidate();
	PRO_CHECK(t.observe(std::string("media.auto.aaaaaaaa"), s, defaults()).has_value()); // reapply after reload
}

void test_describe_block() {
	BlobDetectionProfile d = defaults();
	std::string text = describeProfile("media.auto.aaaaaaaa", "defaults", d);
	PRO_CHECK(text.rfind("[BLOB DETECTION PROFILE]\nmedia=media.auto.aaaaaaaa\nsource=defaults\nthreshold=40", 0) == 0);
}

int main() {
	test_parse_and_lookup();
	test_partial_entry_resolves_to_defaults();
	test_round_trip_is_stable();
	test_update_one_preserves_others();
	test_set_rejects_invalid();
	test_schema_rejection_fails_safe();
	test_invalid_entry_skipped_and_preserved();
	test_missing_file_and_save_load();
	test_tracker_applies_once_per_media_and_restores_defaults();
	test_describe_block();

	std::cout << "blob_detection_profile_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
