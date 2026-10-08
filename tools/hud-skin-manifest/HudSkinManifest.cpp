#include "HudSkinManifest.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace hudskin {

namespace fs = std::filesystem;
using json = nlohmann::json;

bool SkinManifestResult::hasCode(std::string_view code) const {
	return std::any_of(diagnostics.begin(), diagnostics.end(),
		[&](const SkinDiagnostic& d) { return d.code == code; });
}

size_t SkinManifestResult::errorCount() const {
	return static_cast<size_t>(std::count_if(diagnostics.begin(), diagnostics.end(),
		[](const SkinDiagnostic& d) { return d.severity == SkinDiagnosticSeverity::Error; }));
}

namespace {

void addError(SkinManifestResult& r, const char* code, std::string fieldPath, std::string message) {
	r.diagnostics.push_back({SkinDiagnosticSeverity::Error, code, std::move(fieldPath), std::move(message)});
}

// -- Strict JSON: duplicate-key detection ------------------------------------
//
// [ARCH] Duplicate keys rejected. nlohmann's DOM parser silently keeps the
// LAST of two duplicate object keys — ambiguous input for a manifest (two
// different shell paths, say) — so a SAX pre-pass rejects it. Comments are already rejected by
// nlohmann's default (ignore_comments = false) and trailing content after
// the root value is rejected by default (strict = true).
class DuplicateKeyDetector : public nlohmann::json_sax<json> {
public:
	bool duplicateFound = false;
	std::string duplicateKey;

	bool null() override { return true; }
	bool boolean(bool) override { return true; }
	bool number_integer(number_integer_t) override { return true; }
	bool number_unsigned(number_unsigned_t) override { return true; }
	bool number_float(number_float_t, const string_t&) override { return true; }
	bool string(string_t&) override { return true; }
	bool binary(binary_t&) override { return true; }
	bool start_object(std::size_t) override { stack_.emplace_back(); return true; }
	bool end_object() override { stack_.pop_back(); return true; }
	bool start_array(std::size_t) override { return true; }
	bool end_array() override { return true; }
	bool key(string_t& k) override {
		if (!stack_.back().insert(k).second) {
			duplicateFound = true;
			duplicateKey = k;
			return false; // abort parse
		}
		return true;
	}
	bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override {
		return false;
	}

private:
	std::vector<std::set<std::string>> stack_;
};

// -- Field helpers -------------------------------------------------------------

// JSON integer (not float, not bool) in [0, UINT32_MAX].
std::optional<uint32_t> asUint32(const json& v) {
	if (!v.is_number_integer()) return std::nullopt;
	if (v.is_number_unsigned()) {
		auto u = v.get<uint64_t>();
		if (u > UINT32_MAX) return std::nullopt;
		return static_cast<uint32_t>(u);
	}
	return std::nullopt; // negative signed integer
}

const char* jsonTypeName(const json& v) {
	return v.type_name();
}

// [ARCH] skinId: lowercase ASCII slug — letters, digits, internal hyphens
// (consecutive internal hyphens such as "a--b" permitted, Final Schema
// Alignment), no leading/trailing hyphen, non-empty. i.e. first and last
// characters in [a-z0-9], every character in [a-z0-9-]. ASCII-only by
// construction, so Unicode normalization and case folding never arise:
// any non-ASCII byte or uppercase letter is rejected, never normalized.
// No length limit.
bool isLowercaseSlug(const std::string& s) {
	if (s.empty() || s.front() == '-' || s.back() == '-') return false;
	for (char c : s) {
		bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
		if (!ok) return false;
	}
	return true;
}

// One skinVersion component: [ARCH] non-empty ASCII digits, no leading zero
// unless the component is exactly "0", value in [0, UINT32_MAX]. Range is
// checked on the decimal text itself — a canonical (no-leading-zero)
// decimal longer than 10 digits always exceeds 4294967295, and one of at
// most 10 digits fits in uint64 — so arbitrarily long input can never
// overflow or throw. No separate length limit beyond what the range implies.
bool isVersionComponent(std::string_view c) {
	if (c.empty()) return false;
	for (char ch : c) {
		if (ch < '0' || ch > '9') return false;
	}
	if (c.size() > 1 && c.front() == '0') return false; // "01", "00", "003"
	if (c.size() > 10) return false;
	uint64_t value = 0;
	for (char ch : c) value = value * 10 + static_cast<uint64_t>(ch - '0');
	return value <= UINT32_MAX;
}

// [ARCH] skinVersion: exact numeric major.minor.patch — exactly three
// dot-separated components, each validated by isVersionComponent(); nothing
// else (no sign, whitespace, pre-release or build suffix). Never normalized:
// "01.2.3" is rejected, not reinterpreted as "1.2.3".
bool isMajorMinorPatch(const std::string& s) {
	std::string_view rest(s);
	for (int i = 0; i < 3; ++i) {
		size_t dot = rest.find('.');
		bool last = (i == 2);
		if (last != (dot == std::string_view::npos)) return false; // too few / too many dots
		std::string_view component = last ? rest : rest.substr(0, dot);
		if (!isVersionComponent(component)) return false;
		if (!last) rest.remove_prefix(dot + 1);
	}
	return true;
}

// -- Path safety ---------------------------------------------------------------

std::vector<std::string> splitSegments(const std::string& path) {
	std::vector<std::string> segments;
	std::string current;
	for (char c : path) {
		if (c == '/') {
			segments.push_back(current);
			current.clear();
		} else {
			current.push_back(c);
		}
	}
	segments.push_back(current);
	return segments;
}

// Lexical checks only — no filesystem access. Returns the first failing
// code, or nullptr if the path is lexically safe. Order is fixed so the
// reported code is deterministic for inputs that violate several rules.
const char* lexicalPathViolation(const std::string& path) {
	if (path.empty()) return codes::PathEmpty;
	for (unsigned char c : path) {
		if (c < 0x20 || c == 0x7F) return codes::PathControlCharacter;
	}
	// URI / network scheme: "<alpha>[alnum+.-]*:" prefix. Also catches
	// Windows drive-letter paths ("C:...") — both are non-package-relative.
	{
		size_t i = 0;
		if (std::isalpha(static_cast<unsigned char>(path[0]))) {
			i = 1;
			while (i < path.size()) {
				unsigned char c = static_cast<unsigned char>(path[i]);
				if (std::isalnum(c) || c == '+' || c == '.' || c == '-') { ++i; continue; }
				break;
			}
			if (i < path.size() && path[i] == ':') return codes::PathUriScheme;
		}
	}
	if (path.front() == '/') return codes::PathAbsolute;
	// [PROVISIONAL] '\' is a legal filename byte on POSIX but a separator on
	// Windows; rejecting it keeps one package valid-or-invalid on every
	// platform (e.g. "..\\x" can never become a traversal on another host).
	if (path.find('\\') != std::string::npos) return codes::PathBackslash;
	// [ARCH, approved for v1] ANY ".." segment is rejected, not only one
	// that escapes lexically: "dir/../x" resolves differently
	// lexically vs physically when "dir" is a symlink, and rejecting all
	// parent segments removes that whole class of mismatch.
	for (const auto& seg : splitSegments(path)) {
		if (seg == "..") return codes::PathParentTraversal;
	}
	return nullptr;
}

// Filesystem checks for a lexically-safe path. Walks from the (already
// canonical) package root one component at a time using symlink_status()
// (lstat), which never follows a link — so a symlink anywhere in the path
// is detected and rejected WITHOUT touching its target, and nothing
// outside the package root is ever stat'ed or read.
// [ARCH, approved for v1] every symlink is rejected, including dangling ones
// and one whose target stays inside the package — following it to find out
// would mean touching its target.
void checkPathOnDisk(SkinManifestResult& r, const fs::path& canonicalRoot,
	const std::string& relPath, const std::string& fieldPath) {

	std::vector<std::string> segments;
	for (const auto& seg : splitSegments(relPath)) {
		if (seg.empty() || seg == ".") continue;
		segments.push_back(seg);
	}
	if (segments.empty()) {
		addError(r, codes::PathNotRegularFile, fieldPath, "path names the package root, not a file: \"" + relPath + "\"");
		return;
	}

	fs::path current = canonicalRoot;
	for (size_t i = 0; i < segments.size(); ++i) {
		current /= segments[i];
		std::error_code ec;
		fs::file_status st = fs::symlink_status(current, ec);
		if (ec || !fs::exists(st)) {
			addError(r, codes::PathNotFound, fieldPath, "declared file does not exist in package: \"" + relPath + "\"");
			return;
		}
		if (fs::is_symlink(st)) {
			addError(r, codes::PathSymlink, fieldPath, "path component is a symlink (not followed): \"" + relPath + "\"");
			return;
		}
		bool last = (i + 1 == segments.size());
		if (!last && !fs::is_directory(st)) {
			addError(r, codes::PathNotFound, fieldPath, "intermediate path component is not a directory: \"" + relPath + "\"");
			return;
		}
		if (last && !fs::is_regular_file(st)) {
			addError(r, codes::PathNotRegularFile, fieldPath, "declared path is not a regular file: \"" + relPath + "\"");
			return;
		}
	}
}

// -- Asset records -------------------------------------------------------------

enum class AssetClass { Shell, Decoration, Emissive };

const char* assetClassName(AssetClass c) {
	switch (c) {
		case AssetClass::Shell: return "shell";
		case AssetClass::Decoration: return "decoration";
		case AssetClass::Emissive: return "emissive";
	}
	return "";
}

bool classIsRequired(AssetClass c) { return c == AssetClass::Shell; }

std::optional<SkinCanvasAssetRecord> parseCanvasRecord(SkinManifestResult& r, const json& v,
	AssetClass cls, const fs::path* canonicalRoot) {

	const std::string base = std::string("assets.") + assetClassName(cls);
	if (!v.is_object()) {
		addError(r, codes::FieldWrongType, base, std::string("expected object, got ") + jsonTypeName(v));
		return std::nullopt;
	}

	const size_t errorsBefore = r.errorCount();
	SkinCanvasAssetRecord rec;

	// [ARCH] Unknown fields inside a runtime asset record are rejected for
	// the accepted schema subset.
	static const std::set<std::string> kKnownFields = {
		"path", "required", "coordinateSpace", "expectedWidth", "expectedHeight"};
	for (auto it = v.begin(); it != v.end(); ++it) {
		if (!kKnownFields.count(it.key())) {
			addError(r, codes::AssetFieldUnknown, base + "." + it.key(), "unknown field in runtime asset record");
		}
	}

	// path [DRAFT §7.1, §8]
	if (!v.contains("path")) {
		addError(r, codes::FieldMissing, base + ".path", "required field missing");
	} else if (!v["path"].is_string()) {
		addError(r, codes::FieldWrongType, base + ".path", std::string("expected string, got ") + jsonTypeName(v["path"]));
	} else {
		rec.path = v["path"].get<std::string>();
		if (const char* violation = lexicalPathViolation(rec.path)) {
			addError(r, violation, base + ".path", "unsafe package path: \"" + rec.path + "\"");
		} else if (canonicalRoot) {
			checkPathOnDisk(r, *canonicalRoot, rec.path, base + ".path");
		}
	}

	// required — [ARCH] asset-class policy is authoritative: shell required,
	// other classes optional; a conflicting declaration is rejected.
	// [PROVISIONAL] omitting the flag is allowed (the class policy applies);
	// the ruling only addresses conflicting declarations. Existing behavior.
	if (v.contains("required")) {
		if (!v["required"].is_boolean()) {
			addError(r, codes::FieldWrongType, base + ".required", std::string("expected boolean, got ") + jsonTypeName(v["required"]));
		} else if (v["required"].get<bool>() != classIsRequired(cls)) {
			addError(r, codes::AssetRequiredFlagMismatch, base + ".required",
				std::string("asset class '") + assetClassName(cls) + "' is " +
				(classIsRequired(cls) ? "required" : "optional") + " by contract");
		}
	}

	// coordinateSpace — [ARCH] required, and must be exactly "canvas" for
	// full-canvas image assets (shell/decoration/emissive). Case- and
	// whitespace-sensitive: no normalization.
	if (!v.contains("coordinateSpace")) {
		addError(r, codes::FieldMissing, base + ".coordinateSpace", "required field missing");
	} else if (!v["coordinateSpace"].is_string()) {
		addError(r, codes::FieldWrongType, base + ".coordinateSpace",
			std::string("expected string, got ") + jsonTypeName(v["coordinateSpace"]));
	} else if (v["coordinateSpace"].get<std::string>() != "canvas") {
		// [DRAFT §7.3] unknown coordinate enum values are errors.
		addError(r, codes::AssetCoordinateSpaceUnsupported, base + ".coordinateSpace",
			"unsupported coordinate space \"" + v["coordinateSpace"].get<std::string>() + "\" (supported: \"canvas\")");
	}

	// expectedWidth / expectedHeight — [ARCH] LOGICAL canvas dimensions
	// (manifest metadata only; physical texture scaling stays Pi-dependent).
	// [ARCH, Final Schema Alignment] every declared full-canvas asset must
	// declare BOTH, as JSON integers equal to the canonical logical canvas:
	// expectedWidth == 1280, expectedHeight == 720. Missing → FIELD_MISSING,
	// non-integer → FIELD_WRONG_TYPE, any other integer → FIELD_INVALID_VALUE.
	// No image is opened, so these are never compared with decoded pixels.
	auto parseDim = [&](const char* name, uint32_t required, std::optional<uint32_t>& out) {
		if (!v.contains(name)) {
			addError(r, codes::FieldMissing, base + "." + name, "required field missing");
			return;
		}
		if (!v[name].is_number_integer()) {
			addError(r, codes::FieldWrongType, base + "." + name, std::string("expected integer, got ") + jsonTypeName(v[name]));
			return;
		}
		auto d = asUint32(v[name]);
		if (!d || *d != required) {
			addError(r, codes::FieldInvalidValue, base + "." + name,
				"logical canvas dimension must be " + std::to_string(required) + ", got " + v[name].dump());
			return;
		}
		out = *d;
	};
	parseDim("expectedWidth", kCanvasLogicalWidth, rec.expectedWidth);
	parseDim("expectedHeight", kCanvasLogicalHeight, rec.expectedHeight);

	if (r.errorCount() != errorsBefore) return std::nullopt;
	return rec;
}

// -- Core validation -----------------------------------------------------------

bool checkPolicy(SkinManifestResult& r, const SkinManifestValidationPolicy& policy) {
	if (!policy.expectedBlueprintId || policy.expectedBlueprintId->empty() || !policy.expectedBlueprintVersion) {
		addError(r, codes::PolicyBlueprintUnset, "",
			"caller must supply expectedBlueprintId and expectedBlueprintVersion; no canonical default exists");
		return false;
	}
	return true;
}

void validateImpl(SkinManifestResult& r, std::string_view text,
	const SkinManifestValidationPolicy& policy, const fs::path* canonicalRoot) {

	// 1. Strict JSON.
	{
		DuplicateKeyDetector detector;
		bool ok = json::sax_parse(text, &detector);
		if (detector.duplicateFound) {
			addError(r, codes::JsonDuplicateKey, detector.duplicateKey, "duplicate object key in manifest JSON");
			return;
		}
		if (!ok) {
			addError(r, codes::JsonMalformed, "", "manifest is not well-formed JSON");
			return;
		}
	}
	json root = json::parse(text, nullptr, /*allow_exceptions=*/false);
	if (root.is_discarded()) {
		addError(r, codes::JsonMalformed, "", "manifest is not well-formed JSON");
		return;
	}
	if (!root.is_object()) {
		addError(r, codes::JsonRootNotObject, "", std::string("manifest root must be an object, got ") + jsonTypeName(root));
		return;
	}

	SkinManifest m;

	// 2. Schema version — gates everything else: an unsupported version's
	// remaining fields have unknown semantics, so validation stops here.
	if (!root.contains("schemaVersion")) {
		addError(r, codes::FieldMissing, "schemaVersion", "required field missing");
		return;
	}
	if (!root["schemaVersion"].is_number_integer()) {
		addError(r, codes::FieldWrongType, "schemaVersion",
			std::string("expected integer, got ") + jsonTypeName(root["schemaVersion"]));
		return;
	}
	{
		auto sv = asUint32(root["schemaVersion"]);
		bool supported = sv && std::find(std::begin(kSupportedSchemaVersions), std::end(kSupportedSchemaVersions), *sv)
			!= std::end(kSupportedSchemaVersions);
		if (!supported) {
			addError(r, codes::SchemaVersionUnsupported, "schemaVersion",
				"unsupported schema version " + root["schemaVersion"].dump() + " (supported: 1)");
			return;
		}
		m.schemaVersion = *sv;
	}

	// 3. Identity strings.
	auto requireString = [&](const char* name, std::string& out) -> bool {
		if (!root.contains(name)) {
			addError(r, codes::FieldMissing, name, "required field missing");
			return false;
		}
		if (!root[name].is_string()) {
			addError(r, codes::FieldWrongType, name, std::string("expected string, got ") + jsonTypeName(root[name]));
			return false;
		}
		out = root[name].get<std::string>();
		return true;
	};

	if (requireString("skinId", m.skinId) && !isLowercaseSlug(m.skinId)) {
		addError(r, codes::FieldInvalidValue, "skinId", "skinId must be a lowercase ASCII slug: \"" + m.skinId + "\"");
	}
	// [PROVISIONAL] "required" interpreted as non-empty.
	if (requireString("displayName", m.displayName) && m.displayName.empty()) {
		addError(r, codes::FieldInvalidValue, "displayName", "displayName must not be empty");
	}
	if (requireString("skinVersion", m.skinVersion) && !isMajorMinorPatch(m.skinVersion)) {
		addError(r, codes::FieldInvalidValue, "skinVersion", "skinVersion must be \"major.minor.patch\": \"" + m.skinVersion + "\"");
	}

	// 4. Blueprint compatibility — exact match against CALLER-supplied
	// identity (draft §5.4); there is no built-in expected value.
	if (requireString("blueprintId", m.blueprintId) && m.blueprintId != *policy.expectedBlueprintId) {
		addError(r, codes::BlueprintIdMismatch, "blueprintId",
			"package targets blueprint \"" + m.blueprintId + "\", expected \"" + *policy.expectedBlueprintId + "\"");
	}
	if (!root.contains("blueprintVersion")) {
		addError(r, codes::FieldMissing, "blueprintVersion", "required field missing");
	} else if (!root["blueprintVersion"].is_number_integer()) {
		addError(r, codes::FieldWrongType, "blueprintVersion",
			std::string("expected integer, got ") + jsonTypeName(root["blueprintVersion"]));
	} else {
		auto bv = asUint32(root["blueprintVersion"]);
		if (!bv) {
			addError(r, codes::FieldInvalidValue, "blueprintVersion", "expected a non-negative integer");
		} else if (*bv != *policy.expectedBlueprintVersion) {
			addError(r, codes::BlueprintVersionMismatch, "blueprintVersion",
				"package targets blueprint version " + std::to_string(*bv) + ", expected " +
				std::to_string(*policy.expectedBlueprintVersion));
		} else {
			m.blueprintVersion = *bv;
		}
	}

	// 5. Unknown top-level fields — [ARCH] rejected by default. "Explicitly
	// named metadata extensions only after review": none has been reviewed,
	// so there is deliberately no allow-list and no extension namespace
	// (e.g. "x-*", "$schema", "metadata" are all rejected like any other key).
	static const std::set<std::string> kKnownTopLevel = {
		"schemaVersion", "skinId", "displayName", "skinVersion", "blueprintId", "blueprintVersion", "assets"};
	for (auto it = root.begin(); it != root.end(); ++it) {
		if (!kKnownTopLevel.count(it.key())) {
			addError(r, codes::UnknownTopLevelField, it.key(),
				"unknown top-level field \"" + it.key() + "\" (no metadata extensions are approved)");
		}
	}

	// 6. Asset inventory [DRAFT §6, §7.1, §7.3].
	if (!root.contains("assets")) {
		addError(r, codes::FieldMissing, "assets", "required field missing");
	} else if (!root["assets"].is_object()) {
		addError(r, codes::FieldWrongType, "assets", std::string("expected object, got ") + jsonTypeName(root["assets"]));
	} else {
		const json& assets = root["assets"];
		bool shellSeen = false;
		std::optional<SkinCanvasAssetRecord> shell;
		// nlohmann::json objects are std::map-backed: iteration is sorted by
		// key, so diagnostic order is deterministic regardless of the order
		// keys appear in the file.
		for (auto it = assets.begin(); it != assets.end(); ++it) {
			const std::string& cls = it.key();
			if (cls == "shell") {
				shellSeen = true;
				shell = parseCanvasRecord(r, it.value(), AssetClass::Shell, canonicalRoot);
			} else if (cls == "decoration") {
				m.decoration = parseCanvasRecord(r, it.value(), AssetClass::Decoration, canonicalRoot);
			} else if (cls == "emissive") {
				m.emissive = parseCanvasRecord(r, it.value(), AssetClass::Emissive, canonicalRoot);
			} else if (cls == "atlas") {
				// [ARCH] TEMPORARY, this foundation subset only: atlas runtime
				// consumption and icon-role mappings are deferred, and optional
				// atlas REMAINS a future Skin Package v1 design direction. Rejected
				// outright — no field inspection, no path checks, no file access.
				addError(r, codes::AtlasUnsupportedInThisIncrement, "assets.atlas",
					"atlas declarations are not supported by this foundation subset (temporary: atlas consumption "
					"and icon-role mappings are deferred); omit 'atlas' from the manifest");
			} else {
				addError(r, codes::AssetClassUnknown, "assets." + cls, "unknown runtime asset class \"" + cls + "\"");
			}
		}
		if (!shellSeen) {
			addError(r, codes::AssetRequiredMissing, "assets.shell", "required asset class 'shell' is not declared");
		} else if (shell) {
			m.shell = *shell;
		}
	}

	if (r.errorCount() == 0) r.manifest = std::move(m);
}

} // namespace

SkinManifestResult validateManifestText(std::string_view manifestJson,
	const SkinManifestValidationPolicy& policy) {
	SkinManifestResult r;
	if (checkPolicy(r, policy)) validateImpl(r, manifestJson, policy, nullptr);
	r.valid = (r.errorCount() == 0);
	return r;
}

SkinManifestResult validateSkinPackage(const fs::path& packageRoot,
	const SkinManifestValidationPolicy& policy) {
	SkinManifestResult r;
	if (!checkPolicy(r, policy)) {
		r.valid = false;
		return r;
	}

	// The package root is the CALLER's selection, so it is resolved with
	// status()/canonical() (following a symlinked root is the caller's
	// choice). Everything BELOW it is checked with symlink_status() only.
	std::error_code ec;
	fs::file_status rootStatus = fs::status(packageRoot, ec);
	if (ec || !fs::exists(rootStatus)) {
		addError(r, codes::PackageRootMissing, "", "package root does not exist: " + packageRoot.string());
		r.valid = false;
		return r;
	}
	if (!fs::is_directory(rootStatus)) {
		addError(r, codes::PackageRootNotDirectory, "", "package root is not a directory: " + packageRoot.string());
		r.valid = false;
		return r;
	}
	fs::path canonicalRoot = fs::canonical(packageRoot, ec);
	if (ec) {
		addError(r, codes::PackageRootMissing, "", "package root could not be resolved: " + packageRoot.string());
		r.valid = false;
		return r;
	}

	const fs::path manifestPath = canonicalRoot / "skin.json";
	fs::file_status manifestStatus = fs::symlink_status(manifestPath, ec);
	if (ec || !fs::exists(manifestStatus)) {
		addError(r, codes::ManifestMissing, "", "skin.json not found in package root");
		r.valid = false;
		return r;
	}
	if (!fs::is_regular_file(manifestStatus)) { // includes "is a symlink"
		addError(r, codes::ManifestNotRegularFile, "", "skin.json is not a regular file (symlinks are not followed)");
		r.valid = false;
		return r;
	}

	std::ifstream in(manifestPath, std::ios::binary);
	if (!in) {
		addError(r, codes::ManifestUnreadable, "", "skin.json could not be opened");
		r.valid = false;
		return r;
	}
	std::ostringstream buf;
	buf << in.rdbuf();
	if (in.bad()) {
		addError(r, codes::ManifestUnreadable, "", "skin.json could not be read");
		r.valid = false;
		return r;
	}

	validateImpl(r, buf.str(), policy, &canonicalRoot);
	r.valid = (r.errorCount() == 0);
	return r;
}

} // namespace hudskin
