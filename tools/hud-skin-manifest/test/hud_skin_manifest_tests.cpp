// Headless, openFrameworks-free tests for tools/hud-skin-manifest/ —
// same HUD_CHECK convention as shared/src/hud-compositor-test/. No window,
// no GL context, no image decoding.
//
// Packages for negative/filesystem cases are generated at runtime under
// <temp>/hud_skin_manifest_tests/ (wiped before and after) so the repo
// never has to store symlinks. The only on-disk fixture is
// ../fixtures/shell-only-valid/.
//
// Blueprint identity used throughout is a deliberately non-canonical,
// TEST-ONLY value supplied as validator input — no accepted Blueprint
// ID/version exists yet (see ../README.md, "Preflight 1").

#include "../HudSkinManifest.h"

#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace hudskin;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
int g_total = 0;
int g_failures = 0;
void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}
} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

namespace {

// TEST-ONLY identity — matches fixtures/shell-only-valid/skin.json.
const char* kTestBlueprintId = "test-fixture-blueprint-not-canonical";
constexpr uint32_t kTestBlueprintVersion = 1;

SkinManifestValidationPolicy testPolicy() {
	SkinManifestValidationPolicy p;
	p.expectedBlueprintId = kTestBlueprintId;
	p.expectedBlueprintVersion = kTestBlueprintVersion;
	return p;
}

json validManifest() {
	return json{
		{"schemaVersion", 1},
		{"skinId", "fixture-shell-only"},
		{"displayName", "Fixture"},
		{"skinVersion", "0.0.1"},
		{"blueprintId", kTestBlueprintId},
		{"blueprintVersion", kTestBlueprintVersion},
		{"assets", {
			{"shell", {{"path", "shell.png"}, {"required", true}, {"coordinateSpace", "canvas"},
				{"expectedWidth", 1280}, {"expectedHeight", 720}}}}}};
}

// A complete, valid full-canvas asset record (decoration/emissive/shell), so
// a test that adds an optional layer fails only for the reason under test.
json canvasRecord(const std::string& path, std::optional<bool> required = std::nullopt) {
	json rec = {{"path", path}, {"coordinateSpace", "canvas"}, {"expectedWidth", 1280}, {"expectedHeight", 720}};
	if (required) rec["required"] = *required;
	return rec;
}

SkinManifestResult checkText(const json& j) { return validateManifestText(j.dump(), testPolicy()); }
SkinManifestResult checkRaw(const std::string& s) { return validateManifestText(s, testPolicy()); }

// Exactly one Error diagnostic, with this code — the strongest form of
// "this input fails for exactly the reason under test".
bool failsWithOnly(const SkinManifestResult& r, const char* code) {
	return !r.valid && r.errorCount() == 1 && r.hasCode(code);
}

// -- Scratch package helpers ---------------------------------------------------

fs::path scratchRoot() { return fs::temp_directory_path() / "hud_skin_manifest_tests"; }

void writeFile(const fs::path& p, const std::string& contents) {
	fs::create_directories(p.parent_path());
	std::ofstream out(p, std::ios::binary);
	out << contents;
}

// <scratch>/<name>/pkg/ with skin.json = `manifest` and the listed empty files.
fs::path makePackage(const std::string& name, const json& manifest, const std::vector<std::string>& files) {
	fs::path pkg = scratchRoot() / name / "pkg";
	fs::create_directories(pkg);
	writeFile(pkg / "skin.json", manifest.dump(2));
	for (const auto& f : files) writeFile(pkg / f, "");
	return pkg;
}

// -- Positive ------------------------------------------------------------------

void test_fixture_shell_only_is_valid() {
	auto r = validateSkinPackage(HUD_SKIN_FIXTURE_DIR "/shell-only-valid", testPolicy());
	HUD_CHECK(r.valid);
	HUD_CHECK(r.diagnostics.empty());
	HUD_CHECK(r.manifest.has_value());
	if (!r.manifest) return;
	HUD_CHECK(r.manifest->schemaVersion == 1);
	HUD_CHECK(r.manifest->skinId == "fixture-shell-only");
	HUD_CHECK(r.manifest->skinVersion == "0.0.1");
	HUD_CHECK(r.manifest->blueprintId == kTestBlueprintId);
	HUD_CHECK(r.manifest->blueprintVersion == kTestBlueprintVersion);
	HUD_CHECK(r.manifest->shell.path == "shell.png");
	HUD_CHECK(r.manifest->shell.coordinateSpace == SkinCoordinateSpace::Canvas);
	HUD_CHECK(r.manifest->shell.expectedWidth == 1280u);
	HUD_CHECK(r.manifest->shell.expectedHeight == 720u);
	HUD_CHECK(!r.manifest->decoration.has_value());
	HUD_CHECK(!r.manifest->emissive.has_value());
}

void test_text_valid_with_optional_layers() {
	json j = validManifest();
	j["assets"]["decoration"] = canvasRecord("layers/decoration.png", false);
	j["assets"]["emissive"] = canvasRecord("./emissive.png");
	auto r = checkText(j);
	HUD_CHECK(r.valid);
	HUD_CHECK(r.manifest && r.manifest->decoration && r.manifest->decoration->path == "layers/decoration.png");
	HUD_CHECK(r.manifest && r.manifest->emissive && r.manifest->emissive->expectedWidth == 1280u
		&& r.manifest->emissive->expectedHeight == 720u);
}

void test_package_valid_with_optional_layers_on_disk() {
	json j = validManifest();
	j["assets"]["decoration"] = canvasRecord("layers/decoration.png");
	j["assets"]["emissive"] = canvasRecord("emissive.png");
	auto pkg = makePackage("full", j, {"shell.png", "layers/decoration.png", "emissive.png"});
	auto r = validateSkinPackage(pkg, testPolicy());
	HUD_CHECK(r.valid);
	HUD_CHECK(r.diagnostics.empty());
}

// [ARCH] Unknown top-level fields: rejected by default. No metadata extension
// has been reviewed, so extension-looking names get no special treatment.
void test_unknown_top_level_field_rejected() {
	json j = validManifest();
	j["authoringNotes"] = "not allowed";
	auto r = checkText(j);
	HUD_CHECK(failsWithOnly(r, codes::UnknownTopLevelField));
	HUD_CHECK(r.diagnostics.size() == 1 && r.diagnostics[0].fieldPath == "authoringNotes");
	HUD_CHECK(r.diagnostics.size() == 1 && r.diagnostics[0].severity == SkinDiagnosticSeverity::Error);
	HUD_CHECK(!r.manifest.has_value());

	for (const char* key : {"x-authoring", "$schema", "metadata", "extensions", "_comment", "provenance", "preview",
			"atlas", "shell", "SkinId", "skinid", ""}) {
		json k = validManifest();
		k[key] = nullptr;
		auto rk = checkText(k);
		HUD_CHECK(failsWithOnly(rk, codes::UnknownTopLevelField));
		if (!failsWithOnly(rk, codes::UnknownTopLevelField)) std::cerr << "  (top-level key: \"" << key << "\")\n";
	}

	// Several unknown fields: one error each, in sorted key order (deterministic).
	json m = validManifest();
	m["zeta"] = 1; m["alpha"] = 2;
	auto rm = checkText(m);
	HUD_CHECK(!rm.valid && rm.errorCount() == 2);
	HUD_CHECK(rm.diagnostics.size() == 2 && rm.diagnostics[0].fieldPath == "alpha" && rm.diagnostics[1].fieldPath == "zeta");

	// The retired Info code is never emitted.
	HUD_CHECK(!rm.hasCode("SKIN_UNKNOWN_TOP_LEVEL_FIELD_IGNORED"));

	// Unknown top-level field inside an otherwise valid ON-DISK package also rejects it.
	json p = validManifest();
	p["notes"] = "x";
	auto pkg = makePackage("unknown-top-level", p, {"shell.png"});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::UnknownTopLevelField));
}

// No Info diagnostics remain: a valid manifest has an empty diagnostic list.
void test_valid_manifest_has_no_diagnostics() {
	auto r = checkText(validManifest());
	HUD_CHECK(r.valid);
	HUD_CHECK(r.diagnostics.empty());
}

// [ARCH] skinId boundary cases. ASCII-only: no normalization or case folding.
void test_skin_id_boundaries() {
	for (const char* good : {"a", "0", "9", "a1", "1a", "123", "a-b", "a-1-b", "dark-moss", "x0-y9-z",
			// [ARCH, Final Schema Alignment] consecutive internal hyphens permitted:
			"a--b", "a---b", "dark--moss", "a-b--c---d", "0--9"}) {
		json j = validManifest(); j["skinId"] = good;
		auto r = checkText(j);
		HUD_CHECK(r.valid);
		if (!r.valid) std::cerr << "  (skinId expected valid: \"" << good << "\")\n";
	}
	// No length limit: a long valid slug is accepted.
	{
		json j = validManifest(); j["skinId"] = std::string(4096, 'a') + "--" + std::string(4096, '9');
		HUD_CHECK(checkText(j).valid);
	}
	const std::string bad[] = {
		"A", "dArk", "DARK-MOSS", "a--B",      // uppercase: rejected, not folded
		"-", "--", "---", "-a", "a-", "--a", "a--", "-a--b-", // edge hyphens (incl. doubled edges)
		"a_b", "a.b", "a/b", "a b", " a", "a ", "a\tb", "a+b", "a--b!",
		"d\xC3\xA1rk",                          // "dárk" (UTF-8): non-ASCII rejected, not normalized
		"\xEF\xBD\x81",                         // fullwidth 'a' (U+FF41)
		"",
	};
	for (const auto& b : bad) {
		json j = validManifest(); j["skinId"] = b;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldInvalidValue));
		if (!failsWithOnly(r, codes::FieldInvalidValue)) std::cerr << "  (skinId: \"" << b << "\")\n";
	}
}

// [ARCH] skinVersion is exact numeric major.minor.patch.
// [ARCH, Final Schema Alignment] no leading zeros in any component (bare "0"
// allowed); each component independently in [0, 4294967295].
void test_skin_version_boundaries() {
	for (const char* good : {"0.0.0", "0.1.0", "1.0.0", "1.2.3", "10.20.30", "100.200.300",
			"4294967295.0.0", "0.4294967295.0", "0.0.4294967295", "4294967295.4294967295.4294967295",
			"1000000000.0.0", "4294967294.0.0"}) {
		json j = validManifest(); j["skinVersion"] = good;
		auto r = checkText(j);
		HUD_CHECK(r.valid);
		if (!r.valid) std::cerr << "  (skinVersion expected valid: \"" << good << "\")\n";
	}
	// Leading zeros — rejected, never normalized to "1.2.3" etc.
	for (const char* leading : {"01.2.3", "1.02.3", "1.2.03", "1.00.3", "1.2.003", "00.0.0", "0.00.0",
			"0.0.00", "00.00.00", "007.0.0", "01.0.0", "04294967295.0.0"}) {
		json j = validManifest(); j["skinVersion"] = leading;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldInvalidValue));
		if (!failsWithOnly(r, codes::FieldInvalidValue)) std::cerr << "  (leading-zero skinVersion: \"" << leading << "\")\n";
	}
	// Range — each component independently; huge inputs handled without
	// overflow or exceptions (also exercised under ASan/UBSan).
	const std::string huge(5000, '9');
	const std::string outOfRange[] = {
		"4294967296.0.0", "0.4294967296.0", "0.0.4294967296",
		"4294967296.4294967296.4294967296", "9999999999.0.0", "10000000000.0.0",
		"18446744073709551615.0.0", "18446744073709551616.0.0", // uint64 max / max+1
		"99999999999999999999999.0.0", huge + ".0.0", "0." + huge + ".0", "0.0." + huge,
	};
	for (const auto& v : outOfRange) {
		json j = validManifest(); j["skinVersion"] = v;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldInvalidValue));
		if (!failsWithOnly(r, codes::FieldInvalidValue)) std::cerr << "  (out-of-range skinVersion: \"" << v.substr(0, 40) << "\")\n";
	}
	for (const char* bad : {"1", "1.2", "1.2.3.4", ".1.2", "1.2.", "1..2", "+1.2.3", "-1.2.3", "1.2.3-rc1",
			"..", "...", "1.2.3.", ".1.2.3", "0x1.0.0", "1e3.0.0",
			"1.2.3+build", " 1.2.3", "1.2.3 ", "v1.2.3", "1.2.x", "1,2,3", "\xEF\xBC\x91.2.3" /* fullwidth 1 */}) {
		json j = validManifest(); j["skinVersion"] = bad;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldInvalidValue));
		if (!failsWithOnly(r, codes::FieldInvalidValue)) std::cerr << "  (skinVersion: \"" << bad << "\")\n";
	}
	// Never coerced from a JSON number.
	json j = validManifest(); j["skinVersion"] = 1.0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
}

// [ARCH] coordinateSpace required and exactly "canvas", for every full-canvas class.
void test_coordinate_space_all_classes() {
	for (const char* cls : {"shell", "decoration", "emissive"}) {
		json base = validManifest();
		if (std::string(cls) != "shell") base["assets"][cls] = canvasRecord("x.png");
		HUD_CHECK(checkText(base).valid);

		json missing = base; missing["assets"][cls].erase("coordinateSpace");
		HUD_CHECK(failsWithOnly(checkText(missing), codes::FieldMissing));

		for (const char* bad : {"Canvas", "CANVAS", "canvas ", " canvas", "screen", "region", "normalized", ""}) {
			json j = base; j["assets"][cls]["coordinateSpace"] = bad;
			HUD_CHECK(failsWithOnly(checkText(j), codes::AssetCoordinateSpaceUnsupported));
		}
		json wrongType = base; wrongType["assets"][cls]["coordinateSpace"] = nullptr;
		HUD_CHECK(failsWithOnly(checkText(wrongType), codes::FieldWrongType));
	}
}

// [ARCH] Class policy is authoritative for `required`: full matrix.
void test_required_flag_matrix() {
	struct Case { const char* cls; bool flag; bool expectValid; };
	const Case cases[] = {
		{"shell", true, true}, {"shell", false, false},
		{"decoration", false, true}, {"decoration", true, false},
		{"emissive", false, true}, {"emissive", true, false},
	};
	for (const auto& c : cases) {
		json j = validManifest();
		if (std::string(c.cls) != "shell") j["assets"][c.cls] = canvasRecord("x.png");
		j["assets"][c.cls]["required"] = c.flag;
		auto r = checkText(j);
		if (c.expectValid) HUD_CHECK(r.valid);
		else HUD_CHECK(failsWithOnly(r, codes::AssetRequiredFlagMismatch));
	}
	// [PROVISIONAL] omitted flag: class policy applies (existing behavior).
	json j = validManifest();
	j["assets"]["shell"].erase("required");
	j["assets"]["decoration"] = canvasRecord("x.png");
	HUD_CHECK(checkText(j).valid);
	// A decoration flagged required can never substitute for a missing shell.
	json k = validManifest();
	k["assets"].erase("shell");
	k["assets"]["decoration"] = canvasRecord("x.png", true);
	auto rk = checkText(k);
	HUD_CHECK(!rk.valid && rk.errorCount() == 2 && rk.hasCode(codes::AssetRequiredFlagMismatch)
		&& rk.hasCode(codes::AssetRequiredMissing));
}

// [ARCH, Final Schema Alignment] Logical canvas dimensions: every declared
// full-canvas asset must declare expectedWidth == 1280 and expectedHeight ==
// 720 as JSON integers. Metadata only — never compared with image files.
void test_logical_dimension_boundaries() {
	struct Case { json w; json h; const char* code; const char* field; };
	const Case cases[] = {
		// Mismatched integers → INVALID_VALUE on the offending field.
		{640, 360, nullptr, nullptr}, // both wrong: checked separately below (2 errors)
		{1279, 720, codes::FieldInvalidValue, "expectedWidth"},
		{1281, 720, codes::FieldInvalidValue, "expectedWidth"},
		{1280, 719, codes::FieldInvalidValue, "expectedHeight"},
		{1280, 721, codes::FieldInvalidValue, "expectedHeight"},
		{720, 1280, nullptr, nullptr}, // swapped: 2 errors, below
		{1920, 720, codes::FieldInvalidValue, "expectedWidth"},
		{0, 720, codes::FieldInvalidValue, "expectedWidth"},
		{-1280, 720, codes::FieldInvalidValue, "expectedWidth"},
		{4294967296ull + 1280, 720, codes::FieldInvalidValue, "expectedWidth"}, // no uint32 wraparound to 1280
		{1280, 4294967296ull + 720, codes::FieldInvalidValue, "expectedHeight"},
		// Wrong types → WRONG_TYPE, no coercion (1280.0 is not 1280).
		{1280.0, 720, codes::FieldWrongType, "expectedWidth"},
		{1280, 720.0, codes::FieldWrongType, "expectedHeight"},
		{"1280", 720, codes::FieldWrongType, "expectedWidth"},
		{1280, "720", codes::FieldWrongType, "expectedHeight"},
		{true, 720, codes::FieldWrongType, "expectedWidth"},
		{nullptr, 720, codes::FieldWrongType, "expectedWidth"},
		{json::array({1280}), 720, codes::FieldWrongType, "expectedWidth"},
	};
	for (const char* cls : {"shell", "decoration", "emissive"}) {
		const std::string base = std::string("assets.") + cls + ".";
		auto withClass = [&]() {
			json j = validManifest();
			if (std::string(cls) != "shell") j["assets"][cls] = canvasRecord("x.png");
			return j;
		};
		HUD_CHECK(checkText(withClass()).valid); // 1280x720 baseline

		for (const auto& c : cases) {
			json j = withClass();
			j["assets"][cls]["expectedWidth"] = c.w;
			j["assets"][cls]["expectedHeight"] = c.h;
			auto r = checkText(j);
			if (!c.code) {
				HUD_CHECK(!r.valid && r.errorCount() == 2 && r.hasCode(codes::FieldInvalidValue));
				continue;
			}
			bool ok = failsWithOnly(r, c.code) && r.diagnostics.size() == 1 && r.diagnostics[0].fieldPath == base + c.field;
			HUD_CHECK(ok);
			if (!ok) std::cerr << "  (dimension case on " << cls << ": " << c.w.dump() << "x" << c.h.dump() << ")\n";
		}

		// Missing — each dimension individually, then both.
		for (const char* dim : {"expectedWidth", "expectedHeight"}) {
			json j = withClass(); j["assets"][cls].erase(dim);
			auto r = checkText(j);
			HUD_CHECK(failsWithOnly(r, codes::FieldMissing) && r.diagnostics[0].fieldPath == base + dim);
		}
		json both = withClass();
		both["assets"][cls].erase("expectedWidth");
		both["assets"][cls].erase("expectedHeight");
		auto rb = checkText(both);
		HUD_CHECK(!rb.valid && rb.errorCount() == 2 && rb.hasCode(codes::FieldMissing));
	}

	// Combinations: all three classes declared at 1280x720 → valid;
	// one bad class among valid ones → exactly that class's error.
	json all = validManifest();
	all["assets"]["decoration"] = canvasRecord("d.png");
	all["assets"]["emissive"] = canvasRecord("e.png");
	HUD_CHECK(checkText(all).valid);
	for (const char* cls : {"shell", "decoration", "emissive"}) {
		json j = all; j["assets"][cls]["expectedHeight"] = 1080;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldInvalidValue)
			&& r.diagnostics[0].fieldPath == std::string("assets.") + cls + ".expectedHeight");
	}
	json allBad = all;
	for (const char* cls : {"shell", "decoration", "emissive"}) allBad["assets"][cls]["expectedWidth"] = 640;
	auto rab = checkText(allBad);
	HUD_CHECK(!rab.valid && rab.errorCount() == 3);
	// Diagnostic order is deterministic: assets iterate in sorted key order.
	HUD_CHECK(rab.diagnostics.size() == 3 && rab.diagnostics[0].fieldPath == "assets.decoration.expectedWidth"
		&& rab.diagnostics[1].fieldPath == "assets.emissive.expectedWidth"
		&& rab.diagnostics[2].fieldPath == "assets.shell.expectedWidth");

	// On disk: logical dimensions are checked from the manifest only — the
	// 0-byte placeholder files are never opened or decoded.
	json disk = all;
	auto pkg = makePackage("logical-dims", disk, {"shell.png", "d.png", "e.png"});
	HUD_CHECK(validateSkinPackage(pkg, testPolicy()).valid);
	disk["assets"]["emissive"]["expectedWidth"] = 7;
	pkg = makePackage("logical-dims-bad", disk, {"shell.png", "d.png", "e.png"});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::FieldInvalidValue));
}

// [ARCH] Blueprint compatibility: exact match against caller input only.
void test_blueprint_exact_match_boundaries() {
	for (const char* near : {"Test-Fixture-Blueprint-Not-Canonical", "test-fixture-blueprint-not-canonical ",
			" test-fixture-blueprint-not-canonical", "test-fixture-blueprint", ""}) {
		json j = validManifest(); j["blueprintId"] = near;
		HUD_CHECK(failsWithOnly(checkText(j), codes::BlueprintIdMismatch));
	}
	json j = validManifest(); j["blueprintVersion"] = 1.0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["blueprintVersion"] = 4294967296ull;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	j = validManifest(); j["blueprintVersion"] = 0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::BlueprintVersionMismatch));
	// Policy check runs first and alone: nothing else is evaluated without it.
	json broken = validManifest(); broken["skinId"] = "BAD"; broken["extra"] = 1;
	HUD_CHECK(failsWithOnly(validateManifestText(broken.dump(), SkinManifestValidationPolicy{}), codes::PolicyBlueprintUnset));
}

// [ARCH] Atlas rejection is temporary but still independent of other errors.
void test_atlas_rejection_coexists_with_other_errors() {
	json j = validManifest();
	j["assets"]["atlas"] = {{"image", "atlas.png"}, {"metadata", "atlas.json"}};
	j["iconRoles"] = json::object();
	auto r = checkText(j);
	HUD_CHECK(!r.valid && r.errorCount() == 2);
	HUD_CHECK(r.hasCode(codes::AtlasUnsupportedInThisIncrement) && r.hasCode(codes::UnknownTopLevelField));
}

void test_diagnostics_are_deterministic_and_key_order_independent() {
	// Two independent errors, written in two different key orders.
	std::string a = R"({"schemaVersion":1,"skinId":"BAD","displayName":"x","skinVersion":"1","blueprintId":"test-fixture-blueprint-not-canonical","blueprintVersion":1,"assets":{"shell":{"path":"/abs.png","coordinateSpace":"canvas","expectedWidth":1280,"expectedHeight":720}}})";
	std::string b = R"({"assets":{"shell":{"expectedHeight":720,"coordinateSpace":"canvas","path":"/abs.png","expectedWidth":1280}},"blueprintVersion":1,"blueprintId":"test-fixture-blueprint-not-canonical","skinVersion":"1","displayName":"x","skinId":"BAD","schemaVersion":1})";
	auto r1 = checkRaw(a), r2 = checkRaw(a), r3 = checkRaw(b);
	HUD_CHECK(r1.errorCount() == 3); // skinId, skinVersion, shell path
	auto sameCodes = [](const SkinManifestResult& x, const SkinManifestResult& y) {
		if (x.diagnostics.size() != y.diagnostics.size()) return false;
		for (size_t i = 0; i < x.diagnostics.size(); ++i) {
			if (x.diagnostics[i].code != y.diagnostics[i].code || x.diagnostics[i].fieldPath != y.diagnostics[i].fieldPath) return false;
		}
		return true;
	};
	HUD_CHECK(sameCodes(r1, r2));
	HUD_CHECK(sameCodes(r1, r3));
}

// -- Policy (Blueprint identity is caller input) ---------------------------------

void test_unset_policy_is_rejected() {
	SkinManifestValidationPolicy none;
	auto r1 = validateManifestText(validManifest().dump(), none);
	HUD_CHECK(failsWithOnly(r1, codes::PolicyBlueprintUnset));

	SkinManifestValidationPolicy idOnly;
	idOnly.expectedBlueprintId = kTestBlueprintId;
	HUD_CHECK(failsWithOnly(validateManifestText(validManifest().dump(), idOnly), codes::PolicyBlueprintUnset));

	SkinManifestValidationPolicy emptyId = testPolicy();
	emptyId.expectedBlueprintId = "";
	HUD_CHECK(failsWithOnly(validateManifestText(validManifest().dump(), emptyId), codes::PolicyBlueprintUnset));

	auto r2 = validateSkinPackage(HUD_SKIN_FIXTURE_DIR "/shell-only-valid", none);
	HUD_CHECK(failsWithOnly(r2, codes::PolicyBlueprintUnset));
}

// -- Strict JSON ---------------------------------------------------------------------

void test_malformed_json() {
	HUD_CHECK(failsWithOnly(checkRaw(""), codes::JsonMalformed));
	HUD_CHECK(failsWithOnly(checkRaw("{"), codes::JsonMalformed));
	HUD_CHECK(failsWithOnly(checkRaw("{\"schemaVersion\": 1,}"), codes::JsonMalformed));
	HUD_CHECK(failsWithOnly(checkRaw(validManifest().dump() + " {}"), codes::JsonMalformed)); // trailing content
	HUD_CHECK(failsWithOnly(checkRaw("// comment\n" + validManifest().dump()), codes::JsonMalformed));
}

void test_duplicate_keys_rejected() {
	std::string dupTop = R"({"schemaVersion":1,"schemaVersion":1})";
	auto r = checkRaw(dupTop);
	HUD_CHECK(failsWithOnly(r, codes::JsonDuplicateKey));
	std::string dupNested = R"({"assets":{"shell":{"path":"a.png","path":"b.png"}}})";
	HUD_CHECK(failsWithOnly(checkRaw(dupNested), codes::JsonDuplicateKey));
}

void test_root_not_object() {
	HUD_CHECK(failsWithOnly(checkRaw("[]"), codes::JsonRootNotObject));
	HUD_CHECK(failsWithOnly(checkRaw("\"skin\""), codes::JsonRootNotObject));
}

// -- Schema version ------------------------------------------------------------------

void test_schema_version() {
	json j = validManifest();
	j.erase("schemaVersion");
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldMissing));

	j = validManifest(); j["schemaVersion"] = "1";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["schemaVersion"] = 1.0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["schemaVersion"] = true;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));

	for (int v : {0, 2, -1}) {
		j = validManifest(); j["schemaVersion"] = v;
		HUD_CHECK(failsWithOnly(checkText(j), codes::SchemaVersionUnsupported));
	}

	// Unsupported version stops validation: other broken fields are NOT reported.
	j = validManifest(); j["schemaVersion"] = 2; j["skinId"] = "BAD"; j.erase("assets");
	HUD_CHECK(failsWithOnly(checkText(j), codes::SchemaVersionUnsupported));
}

// -- Required fields / types / values --------------------------------------------

void test_missing_required_fields() {
	for (const char* f : {"skinId", "displayName", "skinVersion", "blueprintId", "blueprintVersion", "assets"}) {
		json j = validManifest();
		j.erase(f);
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::FieldMissing));
		HUD_CHECK(!r.diagnostics.empty() && r.diagnostics[0].fieldPath == f);
	}
}

void test_wrong_types() {
	json j = validManifest(); j["skinId"] = 7;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["displayName"] = false;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["skinVersion"] = json::array({1, 0, 0});
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["blueprintId"] = nullptr;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["blueprintVersion"] = "1";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"] = json::array();
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"] = "shell.png";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"]["path"] = 3;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"]["required"] = "true";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"]["coordinateSpace"] = 0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"]["expectedWidth"] = "1280";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
	j = validManifest(); j["assets"]["shell"]["expectedHeight"] = 720.5;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldWrongType));
}

void test_invalid_values() {
	for (const char* bad : {"Dark-Moss", "dark_moss", "-dark", "dark-", "", "dark moss"}) {
		json j = validManifest(); j["skinId"] = bad;
		HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	}
	for (const char* good : {"dark-moss", "plywood-marble", "skin2"}) {
		json j = validManifest(); j["skinId"] = good;
		HUD_CHECK(checkText(j).valid);
	}
	for (const char* bad : {"1.0", "v1.0.0", "1.0.0-beta", "1..0", "1.0.0.0", ""}) {
		json j = validManifest(); j["skinVersion"] = bad;
		HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	}
	json j = validManifest(); j["displayName"] = "";
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	j = validManifest(); j["blueprintVersion"] = -1;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	j = validManifest(); j["assets"]["shell"]["expectedWidth"] = 0;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
	j = validManifest(); j["assets"]["shell"]["expectedHeight"] = -720;
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldInvalidValue));
}

// -- Blueprint compatibility ---------------------------------------------------------

void test_blueprint_mismatch() {
	json j = validManifest(); j["blueprintId"] = "hud-blueprint-v1"; // the draft's placeholder is NOT accepted by default
	HUD_CHECK(failsWithOnly(checkText(j), codes::BlueprintIdMismatch));
	j = validManifest(); j["blueprintVersion"] = 2;
	HUD_CHECK(failsWithOnly(checkText(j), codes::BlueprintVersionMismatch));

	// Same manifest is valid when the CALLER supplies that identity — the
	// validator has no built-in notion of which Blueprint is canonical.
	SkinManifestValidationPolicy other;
	other.expectedBlueprintId = "hud-blueprint-v1";
	other.expectedBlueprintVersion = 1;
	json k = validManifest(); k["blueprintId"] = "hud-blueprint-v1";
	HUD_CHECK(validateManifestText(k.dump(), other).valid);
}

// -- Asset inventory ------------------------------------------------------------------

void test_asset_inventory() {
	json j = validManifest(); j["assets"].erase("shell");
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetRequiredMissing));
	j = validManifest(); j["assets"] = json::object();
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetRequiredMissing));

	for (const char* cls : {"panelMask", "mediaMask", "safeAreaMask", "preview", "provenance", "Shell"}) {
		j = validManifest(); j["assets"][cls] = canvasRecord("x.png");
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, codes::AssetClassUnknown));
	}

	j = validManifest(); j["assets"]["shell"].erase("path");
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldMissing));
	j = validManifest(); j["assets"]["shell"].erase("coordinateSpace");
	HUD_CHECK(failsWithOnly(checkText(j), codes::FieldMissing));
	j = validManifest(); j["assets"]["shell"]["coordinateSpace"] = "screen";
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetCoordinateSpaceUnsupported));
	j = validManifest(); j["assets"]["shell"]["coordinateSpace"] = "Canvas";
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetCoordinateSpaceUnsupported));
	j = validManifest(); j["assets"]["shell"]["blend"] = "additive";
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetFieldUnknown));

	j = validManifest(); j["assets"]["shell"]["required"] = false;
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetRequiredFlagMismatch));
	j = validManifest(); j["assets"]["decoration"] = canvasRecord("d.png", true);
	HUD_CHECK(failsWithOnly(checkText(j), codes::AssetRequiredFlagMismatch));
	j = validManifest(); j["assets"]["shell"].erase("required");
	HUD_CHECK(checkText(j).valid);
}

// -- Atlas policy (a): rejected, never inspected -------------------------------------

void test_atlas_rejected_without_inspection() {
	json j = validManifest();
	j["assets"]["atlas"] = {{"image", "atlas.png"}, {"metadata", "atlas.json"}, {"required", false}};
	auto r = checkText(j);
	HUD_CHECK(failsWithOnly(r, codes::AtlasUnsupportedInThisIncrement));

	// Unsafe paths / garbage inside atlas produce NO path or type diagnostics:
	// the declaration is rejected as a whole, its contents never read.
	j = validManifest();
	j["assets"]["atlas"] = {{"image", "/etc/passwd"}, {"metadata", "../../x.json"}, {"bogus", 1}};
	r = checkText(j);
	HUD_CHECK(failsWithOnly(r, codes::AtlasUnsupportedInThisIncrement));
	j = validManifest(); j["assets"]["atlas"] = 42;
	HUD_CHECK(failsWithOnly(checkText(j), codes::AtlasUnsupportedInThisIncrement));

	// On disk: atlas files present still do not make it valid, and are not opened.
	json k = validManifest();
	k["assets"]["atlas"] = {{"image", "atlas.png"}, {"metadata", "atlas.json"}};
	auto pkg = makePackage("atlas", k, {"shell.png", "atlas.png", "atlas.json"});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::AtlasUnsupportedInThisIncrement));
}

// -- Path security: lexical (no filesystem) ---------------------------------------

void test_lexical_path_rules() {
	struct Case { std::string path; const char* code; };
	const Case cases[] = {
		{"", codes::PathEmpty},
		{"/etc/passwd", codes::PathAbsolute},
		{"/shell.png", codes::PathAbsolute},
		{"../shell.png", codes::PathParentTraversal},
		{"../../outside/shell.png", codes::PathParentTraversal},
		{"layers/../shell.png", codes::PathParentTraversal}, // non-escaping still rejected (stricter)
		{"layers/..", codes::PathParentTraversal},
		{"http://example.com/shell.png", codes::PathUriScheme},
		{"https://example.com/shell.png", codes::PathUriScheme},
		{"file:///etc/passwd", codes::PathUriScheme},
		{"smb://host/share/shell.png", codes::PathUriScheme},
		{"data:image/png;base64,AAAA", codes::PathUriScheme},
		{"C:/skins/shell.png", codes::PathUriScheme}, // drive letter: not package-relative
		{"layers\\shell.png", codes::PathBackslash},
		{"..\\shell.png", codes::PathBackslash},
		{std::string("shell") + '\x01' + ".png", codes::PathControlCharacter},
		{std::string("shell") + '\0' + ".png", codes::PathControlCharacter},
	};
	for (const auto& c : cases) {
		json j = validManifest();
		j["assets"]["shell"]["path"] = c.path;
		auto r = checkText(j);
		HUD_CHECK(failsWithOnly(r, c.code));
		if (!failsWithOnly(r, c.code)) std::cerr << "  (path case: \"" << c.path << "\")\n";
	}
	// Allowed lexical forms.
	for (const char* ok : {"shell.png", "layers/shell.png", "./shell.png", "a.b-c_d/shell.png", "..shell.png"}) {
		json j = validManifest();
		j["assets"]["shell"]["path"] = ok;
		HUD_CHECK(checkText(j).valid);
	}
}

// -- Path security + existence: filesystem -------------------------------------------

void test_filesystem_existence() {
	auto pkg = makePackage("missing-shell", validManifest(), {});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathNotFound));

	json j = validManifest();
	j["assets"]["decoration"] = canvasRecord("decoration.png");
	pkg = makePackage("missing-decoration", j, {"shell.png"});
	auto r = validateSkinPackage(pkg, testPolicy());
	HUD_CHECK(failsWithOnly(r, codes::PathNotFound));
	HUD_CHECK(!r.diagnostics.empty() && r.diagnostics[0].fieldPath == "assets.decoration.path");

	pkg = makePackage("shell-is-dir", validManifest(), {});
	fs::create_directories(pkg / "shell.png");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathNotRegularFile));

	j = validManifest(); j["assets"]["shell"]["path"] = "layers/shell.png";
	pkg = makePackage("intermediate-is-file", j, {"layers"});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathNotFound));

	j = validManifest(); j["assets"]["shell"]["path"] = ".";
	pkg = makePackage("path-is-root", j, {});
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathNotRegularFile));
}

void test_symlink_escapes() {
	// <scratch>/symlink/outside/secret.png lives OUTSIDE the package.
	fs::path base = scratchRoot() / "symlink";
	fs::path outside = base / "outside";
	writeFile(outside / "secret.png", "");

	// 1. shell.png -> outside file (existing target).
	auto pkg = makePackage("symlink/file-escape", validManifest(), {});
	fs::create_symlink(outside / "secret.png", pkg / "shell.png");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathSymlink));

	// 2. shell.png -> nonexistent outside target. If the validator followed
	// links it would report PathNotFound; PathSymlink proves the link itself
	// was rejected via lstat without touching its target.
	pkg = makePackage("symlink/dangling", validManifest(), {});
	fs::create_symlink(base / "does-not-exist" / "secret.png", pkg / "shell.png");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathSymlink));

	// 3. Directory component is a symlink to an outside directory.
	json j = validManifest(); j["assets"]["shell"]["path"] = "layers/secret.png";
	pkg = makePackage("symlink/dir-escape", j, {});
	fs::create_directory_symlink(outside, pkg / "layers");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathSymlink));

	// 4. Internal symlink (target inside the package) — also rejected
	// (stricter-than-draft policy; see README).
	pkg = makePackage("symlink/internal", validManifest(), {"real-shell.png"});
	fs::create_symlink(pkg / "real-shell.png", pkg / "shell.png");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg, testPolicy()), codes::PathSymlink));

	// 5. skin.json itself is a symlink to an outside manifest.
	writeFile(outside / "skin.json", validManifest().dump());
	fs::path pkg5 = base / "manifest-link" / "pkg";
	fs::create_directories(pkg5);
	writeFile(pkg5 / "shell.png", "");
	fs::create_symlink(outside / "skin.json", pkg5 / "skin.json");
	HUD_CHECK(failsWithOnly(validateSkinPackage(pkg5, testPolicy()), codes::ManifestNotRegularFile));
}

void test_package_root_failures() {
	HUD_CHECK(failsWithOnly(validateSkinPackage(scratchRoot() / "no-such-package", testPolicy()), codes::PackageRootMissing));

	writeFile(scratchRoot() / "root-is-file", "");
	HUD_CHECK(failsWithOnly(validateSkinPackage(scratchRoot() / "root-is-file", testPolicy()), codes::PackageRootNotDirectory));

	fs::create_directories(scratchRoot() / "empty-package");
	HUD_CHECK(failsWithOnly(validateSkinPackage(scratchRoot() / "empty-package", testPolicy()), codes::ManifestMissing));

	fs::create_directories(scratchRoot() / "manifest-is-dir" / "skin.json");
	HUD_CHECK(failsWithOnly(validateSkinPackage(scratchRoot() / "manifest-is-dir", testPolicy()), codes::ManifestNotRegularFile));

	writeFile(scratchRoot() / "malformed" / "skin.json", "{ not json");
	HUD_CHECK(failsWithOnly(validateSkinPackage(scratchRoot() / "malformed", testPolicy()), codes::JsonMalformed));
}

} // namespace

int main() {
	std::error_code ec;
	fs::remove_all(scratchRoot(), ec);
	fs::create_directories(scratchRoot());

	test_fixture_shell_only_is_valid();
	test_text_valid_with_optional_layers();
	test_package_valid_with_optional_layers_on_disk();
	test_unknown_top_level_field_rejected();
	test_valid_manifest_has_no_diagnostics();
	test_skin_id_boundaries();
	test_skin_version_boundaries();
	test_coordinate_space_all_classes();
	test_required_flag_matrix();
	test_logical_dimension_boundaries();
	test_blueprint_exact_match_boundaries();
	test_atlas_rejection_coexists_with_other_errors();
	test_diagnostics_are_deterministic_and_key_order_independent();
	test_unset_policy_is_rejected();
	test_malformed_json();
	test_duplicate_keys_rejected();
	test_root_not_object();
	test_schema_version();
	test_missing_required_fields();
	test_wrong_types();
	test_invalid_values();
	test_blueprint_mismatch();
	test_asset_inventory();
	test_atlas_rejected_without_inspection();
	test_lexical_path_rules();
	test_filesystem_existence();
	test_symlink_escapes();
	test_package_root_failures();

	fs::remove_all(scratchRoot(), ec); // remove_all never follows symlinks: outside targets are only ever inside scratch

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
