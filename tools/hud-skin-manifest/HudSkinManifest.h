#pragma once

// ============================================================================
// HudSkinManifest.h — NONPRODUCTION skin-manifest parsing + validation
// foundation (HUD Runtime & Validation Studio, "HUD Skin Manifest Validation
// Foundation" increment).
//
// Status: test-only tooling. NOT a skin loader, NOT a frozen contract.
// Foundation provisionally accepted by Architecture; hardened to
// Architecture's bounded schema-subset rulings (HUD-Skin-Foundation-
// Architecture-Authorization-and-Rulings.md). Rule annotations:
//   [ARCH]        — matches an explicit Architecture ruling for this
//                   foundation subset (NOT a Skin Package v1 freeze);
//   [DRAFT]       — from skin-package-loader-contract-v1-first-draft.md,
//                   not separately ruled;
//   [PROVISIONAL] — existing behavior Architecture has not decided; listed
//                   as an open question in README.md. Not frozen.
//
// Why this lives under tools/, not shared/src/: experience_runtime compiles
// HUD sources only through explicit src/ symlinks, Validation Studio sweeps
// shared/src/hud-compositor/ recursively, and eight other sketches sweep ALL
// of shared/src/ recursively. tools/ is swept by no sketch, so no
// openFrameworks build — production or tooling — can pick this file up
// until a later, separately authorized increment moves it deliberately.
//
// OpenFrameworks-independent: plain C++17 + <filesystem> + the header-only
// nlohmann/json.hpp that openFrameworks already ships (libs/json/include/)
// and shared/src/video-playback/MediaCatalogJsonLoader.cpp already uses —
// no new dependency, no OF headers, no GL context, no image decoding.
//
// Explicitly NOT done here (hard non-goals of this increment): texture
// loading, PNG decoding/pixel or dimension inspection, alpha handling, atlas
// metadata parsing, icon-role interpretation, Pi budget enforcement, GPU
// residency, renderer hooks, skin selection/switching, fallback activation.
// ============================================================================

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hudskin {

// -- Diagnostics -------------------------------------------------------------

enum class SkinDiagnosticSeverity : uint8_t {
	Error, // package is rejected
	Info   // recorded for visibility; never rejects (no Info diagnostic is emitted since hardening)
};

// Stable, machine-readable diagnostic codes. Tests assert on these, never
// on message text. Kept as string constants (not an enum) so a diagnostic
// stays self-describing when printed or logged by a future Studio panel.
namespace codes {
	// Caller / policy
	inline constexpr const char* PolicyBlueprintUnset = "SKIN_POLICY_BLUEPRINT_UNSET";
	// Package root / manifest file
	inline constexpr const char* PackageRootMissing = "SKIN_PACKAGE_ROOT_MISSING";
	inline constexpr const char* PackageRootNotDirectory = "SKIN_PACKAGE_ROOT_NOT_DIRECTORY";
	inline constexpr const char* ManifestMissing = "SKIN_MANIFEST_MISSING";
	inline constexpr const char* ManifestNotRegularFile = "SKIN_MANIFEST_NOT_REGULAR_FILE";
	inline constexpr const char* ManifestUnreadable = "SKIN_MANIFEST_UNREADABLE";
	// JSON
	inline constexpr const char* JsonMalformed = "SKIN_JSON_MALFORMED";
	inline constexpr const char* JsonDuplicateKey = "SKIN_JSON_DUPLICATE_KEY";
	inline constexpr const char* JsonRootNotObject = "SKIN_JSON_ROOT_NOT_OBJECT";
	// Fields
	inline constexpr const char* FieldMissing = "SKIN_FIELD_MISSING";
	inline constexpr const char* FieldWrongType = "SKIN_FIELD_WRONG_TYPE";
	inline constexpr const char* FieldInvalidValue = "SKIN_FIELD_INVALID_VALUE";
	// Error. Replaces the foundation's Info-only SKIN_UNKNOWN_TOP_LEVEL_FIELD_IGNORED
	// (Architecture ruling: reject by default; named extensions only after review —
	// none are approved, so no allow-list exists).
	inline constexpr const char* UnknownTopLevelField = "SKIN_UNKNOWN_TOP_LEVEL_FIELD";
	// Schema / Blueprint
	inline constexpr const char* SchemaVersionUnsupported = "SKIN_SCHEMA_VERSION_UNSUPPORTED";
	inline constexpr const char* BlueprintIdMismatch = "SKIN_BLUEPRINT_ID_MISMATCH";
	inline constexpr const char* BlueprintVersionMismatch = "SKIN_BLUEPRINT_VERSION_MISMATCH";
	// Asset inventory
	inline constexpr const char* AssetClassUnknown = "SKIN_ASSET_CLASS_UNKNOWN";
	inline constexpr const char* AssetFieldUnknown = "SKIN_ASSET_FIELD_UNKNOWN";
	inline constexpr const char* AssetRequiredMissing = "SKIN_ASSET_REQUIRED_MISSING";
	inline constexpr const char* AssetRequiredFlagMismatch = "SKIN_ASSET_REQUIRED_FLAG_MISMATCH";
	inline constexpr const char* AssetCoordinateSpaceUnsupported = "SKIN_ASSET_COORDINATE_SPACE_UNSUPPORTED";
	inline constexpr const char* AtlasUnsupportedInThisIncrement = "SKIN_ATLAS_UNSUPPORTED_IN_THIS_INCREMENT";
	// Paths
	inline constexpr const char* PathEmpty = "SKIN_PATH_EMPTY";
	inline constexpr const char* PathAbsolute = "SKIN_PATH_ABSOLUTE";
	inline constexpr const char* PathUriScheme = "SKIN_PATH_URI_SCHEME";
	inline constexpr const char* PathBackslash = "SKIN_PATH_BACKSLASH";
	inline constexpr const char* PathControlCharacter = "SKIN_PATH_CONTROL_CHARACTER";
	inline constexpr const char* PathParentTraversal = "SKIN_PATH_PARENT_TRAVERSAL";
	inline constexpr const char* PathSymlink = "SKIN_PATH_SYMLINK";
	inline constexpr const char* PathNotFound = "SKIN_PATH_NOT_FOUND";
	inline constexpr const char* PathNotRegularFile = "SKIN_PATH_NOT_REGULAR_FILE";
} // namespace codes

struct SkinDiagnostic {
	SkinDiagnosticSeverity severity = SkinDiagnosticSeverity::Error;
	std::string code;      // one of codes::*
	std::string fieldPath; // e.g. "assets.shell.path"; "" for package-level issues
	std::string message;   // human-readable; NOT asserted on by tests
};

// -- Manifest model -----------------------------------------------------------

// [DRAFT §9.2] The only coordinate space this increment recognizes.
enum class SkinCoordinateSpace : uint8_t { Canvas };

// One full-canvas layer record: shell (required), decoration, emissive
// (optional). [DRAFT §6, §7.1]
struct SkinCanvasAssetRecord {
	std::string path;                              // package-relative, validated (§8)
	SkinCoordinateSpace coordinateSpace = SkinCoordinateSpace::Canvas;
	// [ARCH] LOGICAL canvas dimensions — manifest metadata, not decoded PNG
	// pixel size; physical texture scaling stays Pi-dependent.
	// [ARCH, Final Schema Alignment] required on every declared full-canvas
	// asset and must equal kCanvasLogicalWidth x kCanvasLogicalHeight. Kept
	// std::optional only to preserve the model's shape; always set when valid.
	std::optional<uint32_t> expectedWidth;
	std::optional<uint32_t> expectedHeight;
};

struct SkinManifest {
	uint32_t schemaVersion = 0;   // [DRAFT §5.2]
	std::string skinId;           // [DRAFT §5.3] lowercase ASCII slug
	std::string displayName;      // [DRAFT §5.3]
	std::string skinVersion;      // [DRAFT §5.3] "major.minor.patch"
	std::string blueprintId;      // [DRAFT §5.4] exact match vs policy
	uint32_t blueprintVersion = 0; // [DRAFT §5.4] exact match vs policy

	SkinCanvasAssetRecord shell;                     // [DRAFT §6.1] required
	std::optional<SkinCanvasAssetRecord> decoration; // [DRAFT §6.2] optional
	std::optional<SkinCanvasAssetRecord> emissive;   // [DRAFT §6.3] optional
	// No atlas member: atlas declarations are rejected TEMPORARILY, for this
	// foundation subset only (codes::AtlasUnsupportedInThisIncrement).
	// Optional atlas remains a future Skin Package v1 design direction.
};

// -- Validation policy --------------------------------------------------------

// Supplied by the CALLER. Deliberately has no default Blueprint identity:
// no accepted Blueprint ID/version exists yet (HUD-006 BLOCKED; the draft's
// "hud-blueprint-v1"/1 are placeholders, §22.5). Leaving either field unset
// is itself a validation error (codes::PolicyBlueprintUnset), so no caller
// can silently inherit an invented canonical value.
struct SkinManifestValidationPolicy {
	std::optional<std::string> expectedBlueprintId;
	std::optional<uint32_t> expectedBlueprintVersion;
};

// [DRAFT §5.2] The schema versions this foundation understands.
inline constexpr uint32_t kSupportedSchemaVersions[] = {1};

// The canonical 1280x720 logical HUD canvas (FIXED BY EXISTING ARCHITECTURE;
// same value as HudCompositorBridge's canvas — duplicated here, not
// included, because this tool must stay OF-free and outside shared/src).
inline constexpr uint32_t kCanvasLogicalWidth = 1280;
inline constexpr uint32_t kCanvasLogicalHeight = 720;

// -- Result -------------------------------------------------------------------

struct SkinManifestResult {
	bool valid = false; // true iff no Error-severity diagnostic was recorded
	std::vector<SkinDiagnostic> diagnostics; // deterministic order
	std::optional<SkinManifest> manifest;    // populated only when valid

	bool hasCode(std::string_view code) const;
	size_t errorCount() const;
};

// -- Entry points -------------------------------------------------------------

// Pure-text validation: JSON strictness, schema, fields, Blueprint
// compatibility, asset inventory, and LEXICAL path safety. Touches no
// filesystem at all — referenced-file existence and symlink checks are
// skipped (they need a package root; see validateSkinPackage()).
SkinManifestResult validateManifestText(std::string_view manifestJson,
	const SkinManifestValidationPolicy& policy);

// Full package validation for one package directory: package-root checks,
// reads `<packageRoot>/skin.json`, runs validateManifestText()'s checks, then
// for every lexically-safe declared asset path walks its components from the
// package root with symlink_status() (lstat — never follows a link), so no
// filesystem metadata outside the package root is ever touched.
// Reads exactly one file's CONTENTS: skin.json. Never opens asset files.
SkinManifestResult validateSkinPackage(const std::filesystem::path& packageRoot,
	const SkinManifestValidationPolicy& policy);

} // namespace hudskin
