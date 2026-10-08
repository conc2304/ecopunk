# hud-skin-manifest — NONPRODUCTION skin manifest validation foundation

**Status, 2026-10-08:**
- Foundation **provisionally accepted** by Architecture.
- **Hardened** to Architecture's bounded schema-subset rulings (`HUD-Skin-Foundation-Architecture-Authorization-and-Rulings.md`).
- **Final Schema Alignment ACCEPTED** by Architecture as a bounded tools-only implementation (`docs/shared-project-docs/Architecture-Acceptance-HUD-Skin-Manifest-Final-Schema-Alignment.md`).
- **Not a loader. Skin Package v1 is NOT frozen.** HUD-006 (Blueprint identity) still blocks production compatibility.
- No openFrameworks app compiles this code.

What it does: parses and validates one skin package's `skin.json`, and checks that the declared files exist inside the package.

What it does **not** do:
- load or decode images, or check alpha or pixel dimensions;
- read atlas data or icon roles;
- enforce Pi limits;
- touch the GPU or the renderer;
- select skins, or fall back from a failed one.

## Layout

```text
tools/hud-skin-manifest/
  HudSkinManifest.h / .cpp        model, policy, diagnostics, validateManifestText(), validateSkinPackage()
  fixtures/shell-only-valid/      skin.json + shell.png
  test/hud_skin_manifest_tests.cpp, test/Makefile.tests
  .gitignore                      ignores the generated test binary only
```

Dependencies: C++17, `<filesystem>`, and the `nlohmann/json.hpp` header that openFrameworks already ships in `libs/json/include/`. No openFrameworks headers and no new dependency.

**The fixture's `shell.png` is an empty (0-byte) placeholder.** It exists only so the file-existence check has something to find. It is not image data, and it proves nothing about PNG decoding, dimensions or alpha.

## Run

```bash
cd tools/hud-skin-manifest/test
make -f Makefile.tests test          # → "394/394 checks passed."
```

Sanitizer configuration, as used for the hardening evidence. Run it from `test/` and write the binary anywhere outside the repo:

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I../../../../../../libs/json/include -DHUD_SKIN_FIXTURE_DIR="\"$(cd ../fixtures && pwd)\"" \
  hud_skin_manifest_tests.cpp ../HudSkinManifest.cpp -o "$TMPDIR/skin_tests_asan" && "$TMPDIR/skin_tests_asan"
```

The tests create temporary packages, including the symlink cases, under `$TMPDIR/hud_skin_manifest_tests/`, and remove them before and after the run.

## Build isolation (why `tools/`)

| Host | Compiles this code? | Why |
|---|---|---|
| `experience_runtime` (production) | **No** | It compiles HUD sources only through symlinks in `src/`, and none points into `tools/` |
| `hud_validation_studio` | **No** | Its sweep is `shared/src/hud-compositor/` only |
| 8 sketches that sweep all of `shared/src/` | **No** | `tools/` is outside `shared/src/` |
| `test/Makefile.tests` | **Yes** | Explicit file list; this is the only builder |

Being under `tools/` does **not** prove the code can later reach production. A future move must cover both ExperienceRuntime's symlink-based build and Validation Studio's recursive sweep. That move is a separately gated increment.

## Accepted foundation schema subset (schemaVersion 1)

Legend:
- **ARCH:** matches an explicit Architecture ruling for this subset.
- **DRAFT:** taken from the loader-contract first draft, not separately ruled.
- **PROVISIONAL:** existing behavior that Architecture has not decided. Preserved and pinned by tests, not frozen.

**Blueprint compatibility (ARCH)**
- The caller supplies the expected ID and version through `SkinManifestValidationPolicy`. There is **no default**.
- If either is missing, validation fails closed with `SKIN_POLICY_BLUEPRINT_UNSET`, and that is the only error reported.
- The manifest's `blueprintId` must match the caller's value exactly: case- and whitespace-sensitive, uint32 version.
- The fixture identity `test-fixture-blueprint-not-canonical` / `1` is **test-only**. The canonical identity is **unresolved (HUD-006)**.

**JSON**
- Duplicate keys rejected. **ARCH**
- Comments and trailing content rejected. Nlohmann strict default.
- The root must be an object.

**Top-level fields**
- `schemaVersion` must be an integer in {1}; anything else stops validation. **DRAFT**
- Unknown top-level fields are rejected with `SKIN_UNKNOWN_TOP_LEVEL_FIELD`. **ARCH**
- No metadata extension has been reviewed, so there is **no allow-list and no extension namespace**: `x-*`, `$schema`, `metadata` and the like are all rejected.

**`skinId` (ARCH)**
- Lowercase ASCII slug, non-empty. The first and last characters are `[a-z0-9]`, and every character is `[a-z0-9-]`.
- Consecutive internal hyphens are **permitted**: `a--b`, `a---b` (Final Schema Alignment).
- Edge hyphens are rejected: `-a`, `a-`, `--a`, `a--`.
- ASCII-only, so it is never normalized or case-folded. Uppercase, Unicode, full-width characters, whitespace and other punctuation are rejected.
- No length limit.

**`skinVersion` (ARCH, Final Schema Alignment)**
- Exactly three dot-separated decimal components: no sign, whitespace or suffix.
- **No leading zeros** in any component, except a bare `0`. `0.0.0`, `0.1.0` and `1.0.0` are valid; `01.2.3`, `1.00.3` and `1.2.003` are rejected, never normalized.
- **Each component independently in [0, 4294967295].** `4294967295` is accepted in every position; `4294967296` in any position is rejected.
- Range is checked on the text, so even very long inputs (5000 digits are tested) cannot overflow or throw. There is no separate length limit.

**`displayName`**
- Required, non-empty string. **DRAFT**; "non-empty" is **PROVISIONAL**.

**Asset classes (ARCH)**
- Known classes: `shell` (required), `decoration` and `emissive` (optional).
- Unknown classes are rejected, including `panelMask`, `mediaMask`, `safeAreaMask`, `preview` and `provenance`.

**Asset record fields (ARCH)**
- Allowed fields: `path`, `required`, `coordinateSpace`, `expectedWidth`, `expectedHeight`.
- Any other field is rejected.

**`required` (ARCH)**
- The class policy is authoritative, and a flag that conflicts with it is rejected.
- Omitting the flag is allowed (**PROVISIONAL**; the ruling covers only conflicting declarations).

**`coordinateSpace` (ARCH)**
- Required, and must be exactly `"canvas"` for shell, decoration and emissive.
- No case or whitespace normalization.

**`expectedWidth` / `expectedHeight` (ARCH, Final Schema Alignment)**
- These are **logical canvas dimensions**: manifest metadata, not decoded PNG size. Physical texture scaling stays Pi-dependent.
- **Required on every declared** `shell`, `decoration` and `emissive` asset, and they must equal exactly **1280** and **720** as JSON integers.
- Diagnostics:
  - missing → `SKIN_FIELD_MISSING`;
  - non-integer, including `1280.0`, `"1280"`, `true` and `null` → `SKIN_FIELD_WRONG_TYPE`, with no coercion;
  - any other integer, including `0`, negatives, swapped `720×1280`, and `1280 + 2^32` (no wraparound) → `SKIN_FIELD_INVALID_VALUE`.
- Never compared with an image file.

**Declared optional asset with a missing file**
- Rejected. **DRAFT** §15.4.

**Atlas: temporary rejection, this subset only (ARCH)**
- Any `assets.atlas` declaration fails with `SKIN_ATLAS_UNSUPPORTED_IN_THIS_INCREMENT`. Its contents are never read: no field checks, no path checks, no file access.
- Runtime atlas consumption and icon-role mappings are deferred.
- This is **not** a ruling against atlas. Optional atlas **remains a future Skin Package v1 design direction**.
- No icon-role catalog exists, and none was created.

## Path security (ARCH: all `..` segments and all symlinks rejected for v1)

| Input | Result |
|---|---|
| `""` | `SKIN_PATH_EMPTY` |
| `/etc/passwd`, `/shell.png` | `SKIN_PATH_ABSOLUTE` |
| `http://`, `https://`, `file://`, `smb://`, `data:`, `C:/…` | `SKIN_PATH_URI_SCHEME` |
| `layers\shell.png`, `..\shell.png` | `SKIN_PATH_BACKSLASH` (PROVISIONAL; not covered by a ruling) |
| control characters, including NUL | `SKIN_PATH_CONTROL_CHARACTER` |
| `../x`, `layers/../x`, `layers/..` | `SKIN_PATH_PARENT_TRAVERSAL` |
| any symlinked file or directory component, including **dangling** links and links whose target is inside the package | `SKIN_PATH_SYMLINK` |
| `skin.json` is a symlink | `SKIN_MANIFEST_NOT_REGULAR_FILE` |
| missing file / directory where a file is expected / file used as a directory / `.` | `SKIN_PATH_NOT_FOUND` / `SKIN_PATH_NOT_REGULAR_FILE` |
| `./shell.png`, `layers/shell.png`, `..shell.png` | allowed |

**How reads stay inside the package:**
- The package root is resolved once with `canonical()`. That is the caller's selection.
- Everything below the root is walked one component at a time with `symlink_status()` (lstat), which never follows a link. The dangling-link test proves it: it reports `SYMLINK`, not `NOT_FOUND`.
- The only file whose contents are read is `skin.json`.

## Diagnostic codes

`SKIN_POLICY_BLUEPRINT_UNSET`
`SKIN_PACKAGE_ROOT_MISSING`, `SKIN_PACKAGE_ROOT_NOT_DIRECTORY`, `SKIN_MANIFEST_MISSING`, `SKIN_MANIFEST_NOT_REGULAR_FILE`, `SKIN_MANIFEST_UNREADABLE`
`SKIN_JSON_MALFORMED`, `SKIN_JSON_DUPLICATE_KEY`, `SKIN_JSON_ROOT_NOT_OBJECT`
`SKIN_FIELD_MISSING`, `SKIN_FIELD_WRONG_TYPE`, `SKIN_FIELD_INVALID_VALUE`, `SKIN_UNKNOWN_TOP_LEVEL_FIELD`
`SKIN_SCHEMA_VERSION_UNSUPPORTED`, `SKIN_BLUEPRINT_ID_MISMATCH`, `SKIN_BLUEPRINT_VERSION_MISMATCH`
`SKIN_ASSET_CLASS_UNKNOWN`, `SKIN_ASSET_FIELD_UNKNOWN`, `SKIN_ASSET_REQUIRED_MISSING`, `SKIN_ASSET_REQUIRED_FLAG_MISMATCH`, `SKIN_ASSET_COORDINATE_SPACE_UNSUPPORTED`, `SKIN_ATLAS_UNSUPPORTED_IN_THIS_INCREMENT`
`SKIN_PATH_EMPTY`, `SKIN_PATH_ABSOLUTE`, `SKIN_PATH_URI_SCHEME`, `SKIN_PATH_BACKSLASH`, `SKIN_PATH_CONTROL_CHARACTER`, `SKIN_PATH_PARENT_TRAVERSAL`, `SKIN_PATH_SYMLINK`, `SKIN_PATH_NOT_FOUND`, `SKIN_PATH_NOT_REGULAR_FILE`

**Retired in hardening:** `SKIN_UNKNOWN_TOP_LEVEL_FIELD_IGNORED`, which was Info-only. It is replaced by the Error code `SKIN_UNKNOWN_TOP_LEVEL_FIELD`. No Info diagnostic is emitted any more.

**Final Schema Alignment:** no codes were added, renamed or retired. The four new rules reuse `SKIN_FIELD_INVALID_VALUE`, `SKIN_FIELD_MISSING` and `SKIN_FIELD_WRONG_TYPE`.

## Evidence (2026-10-08, macOS, Apple clang 17)

**Run (after Final Schema Alignment):**
- `make -f Makefile.tests test` → **394/394** passed, 0 warnings with `-Wall -Wextra`.
- The sanitizer command above (`-Wpedantic`, ASan + UBSan, `UBSAN_OPTIONS=halt_on_error=1`) → **394/394** passed, 0 build warnings, build and run exit 0.

**Not run:** any openFrameworks app build, the existing HUD suites, the GL harness, Validation Studio, the Pi, or mutation testing. Nothing outside this directory changed.

**This does not prove:** image decoding, alpha or dimension correctness, GPU or runtime integration, Pi performance, a schema freeze, or production acceptance.

## Open Architecture questions

Resolved by the Final Schema Alignment ruling and now implemented:
- `skinVersion` leading zeros and per-component range;
- 1280×720 dimension equality;
- consecutive internal hyphens in `skinId`.

Still open:
1. **Blueprint identity (HUD-006), blocker for production:** the canonical `blueprintId` / `blueprintVersion`, and whether production may ever default them.
2. **Metadata extensions:** which named extensions, if any, will be reviewed, and under what key shape? Today there are none and no namespace. Deferred.
3. **Atlas and icon roles:** the owner and schema for the future atlas metadata and the runtime icon-role catalog. Deferred.

**Closed:** dimension presence. Architecture confirmed that `expectedWidth` (= 1280) and `expectedHeight` (= 720) are mandatory on every declared shell, decoration and emissive asset. The Final Schema Alignment was **ACCEPTED** by Architecture as a bounded tools-only implementation. See `docs/shared-project-docs/Architecture-Acceptance-HUD-Skin-Manifest-Final-Schema-Alignment.md`.

The rule set for the omitted `required` flag (permitted), backslash paths (rejected) and `displayName` (non-empty, no maximum) was preserved as instructed by the Final Schema Alignment authorization.
