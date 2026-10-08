# HUD-006 / HUD-004 Source Verification Report

**To:** HUD Runtime & Validation Studio Manager; for Architecture & Program Coordination (CC HUD Art)
**From:** HUD Runtime coding agent
**Date:** 2026-10-08
**Mode:** Read-only. No file edits, builds, test runs, staging, commits or pushes.

**Disposition:**
- **The region count is resolved: 19, verified from source.**
- **No canonical Blueprint identity is evidenced. HUD-006 stays blocked.**
- **One new production-rendering discrepancy needs an Architecture decision before any freeze (§2.4).**

**Evidence tags used in this report:**

| Tag | Meaning |
|---|---|
| `SOURCE VERIFIED` | Read in current tracked source at the cited revision. |
| `DOCUMENT ACCEPTED` | An acceptance statement exists in a tracked record. |
| `DOCUMENT ASSERTED` | Stated in a tracked review document, with no acceptance of its own. |
| `AGENT-REPORTED` | Reported by an earlier session and not re-run here. |
| `DRAFT` | A first draft or proposal. |
| `NOT EVIDENCED` | Nothing found in the stated search scope. |
| `MISSING` | A required artifact or capability is absent. |

---

## 1. Provenance

**Repository:** `/Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage`
- Branch: `blueprint-emergence-updates`
- HEAD: `dfbe16959911121cc44d738326a366b5edfc17a1`
- `origin/blueprint-emergence-updates` = `61a4acf`; the local branch is **3 commits ahead and unpushed** (`1e2b941`, `4bc1be7`, `dfbe169`).
- `git status --short`: **clean** (0 entries).

**Local commits:** `SOURCE VERIFIED`

| Commit | Content | On a remote? |
|---|---|---|
| `4bc1be7` | `tools/hud-skin-manifest/` (8 files) plus `docs/shared-project-docs/Architecture-Acceptance-HUD-Skin-Manifest-Final-Schema-Alignment.md` | No |
| `dfbe169` | `docs/shared-project-docs/03-work-registry.md`, HUD-004 only (+13/−5) | No |

These local commits are **not durable** under the registry's interim rule, which requires a commit pushed to `origin`.

**HUD runtime revision stability:** `SOURCE VERIFIED`
- `git diff --stat 61a4acf HEAD` over `shared/src/hud-compositor`, `hud-compositor-test`, `hud-runtime`, `sketches/hud_validation_studio`, the bridge files, and the two review documents is **empty**.
- The earlier probe at `61a4acf` therefore describes the same HUD source as HEAD.
- Line numbers below were re-read at HEAD.

**File revisions** (`git log -1 -- <path>`):

| Path | Last change |
|---|---|
| `shared/src/hud-compositor/{HudRegionCatalog,HudWireframeRenderer,MediaViewportMesh}.{h,cpp}`, `widgets/HudTextMetricsCache.*`, `widgets/HudWidgetDrawUtils.h` | `557edc3` 2026-08-08 |
| `sketches/experience_runtime/src/HudCompositorBridge.{h,cpp}`, `hud_typography_tests.cpp`, `hud_media_viewport_tests.cpp`, `sketches/hud_validation_studio/config.make`, `Scene-HUD-Contract-v1.md` | `557edc3` 2026-08-08 |
| `shared/src/hud-compositor-test/hud_presentation_tests.cpp` | `d25cc13` 2026-08-10 |
| `docs/reviews/hud-wireframe-bounds-v1.md`, `hud-typography-metrics-v1.md`, `hud-runtime-architecture-closure-review.md`, `hud-final-narrow-closure-patch-report.md` | `d25cc13` 2026-08-10 |
| `docs/shared-project-docs/Architecture-Authorization-Temporal-Production-Scene-2.md` | `d25cc13` 2026-08-10 |
| `docs/shared-project-docs/Decision-Log-v3.md` | `59c24f4` 2026-10-05 |
| `sketches/experience_runtime/src/ExperienceRuntime.cpp` | `1e2b941` 2026-10-08 (HUD call sites unchanged in position) |
| `sketches/experience_runtime/config.make` | `6cefb46` 2026-10-06 |
| `docs/shared-project-docs/03-work-registry.md` | `dfbe169` 2026-10-08 |

**Inputs not available** (exact names searched in the repo and `~/Downloads`):

| Input | Impact |
|---|---|
| Architecture's "HUD-006 Evidence and HUD-004 Contract Reconciliation" handoff (mandatory input 1) | None of the off-repo HUD-006 files is an Architecture document. They are HUD Art reports and a HUD Runtime manager bundle. This report therefore follows the prompt and the bundle only, and makes no Architecture-authored rulings. |
| `hud-blueprint-package-v1(2).md` | Its historical illustrative IDs (`media.viewport`, `hud.title`) could not be checked first-hand. |
| `Pasted markdown(20260809-222948).md` (reconciled region map) and `Pasted markdown(20261008-181025).md` (the probe, as named in the bundle) | Not on disk under those names. The probe content is this agent's own earlier report. |

**Limitations:**
- Static inspection only. No test, Studio or GL run.
- Runtime catalog cardinality is proven from source structure, not by running code (§2.2).

---

## 2. Authoritative region table and count

### 2.1 Every registration, in constructor order — `SOURCE VERIFIED` (`HudRegionCatalog.cpp`, `557edc3`)

Pixel values are the renderer's own arithmetic, `normalized × canvas` in `float` (`HudWireframeRenderer.cpp:78-86`, canvas 1280×720 set by `HudCompositorBridge.cpp:6`). They were reproduced in IEEE-754 single precision and shown to 1 decimal place; the float rounding error is below 0.0001 px in every case.

| # | Line | ID | Role | Normalized (x, y, w, h) | 1280×720 px (x, y, w, h) | Max bindings |
|---|---|---|---|---|---|---|
| 1 | 32 | `universal.scene_title` | Universal | 0.66, 0.03, 0.32, 0.06 | 844.8, 21.6, 409.6, 43.2 | 1 |
| 2 | 35 | `universal.media_title` | Universal | 0.66, 0.10, 0.32, 0.05 | 844.8, 72.0, 409.6, 36.0 | 1 |
| 3 | 38 | `universal.primary_state` | Universal | 0.66, 0.16, 0.32, 0.06 | 844.8, 115.2, 409.6, 43.2 | 1 |
| 4 | 41 | `universal.activity` | Universal | 0.66, 0.23, 0.32, 0.06 | 844.8, 165.6, 409.6, 43.2 | 1 |
| 5 | 51 | `universal.effect_summary` | Universal | 0.66, 0.30, 0.32, 0.06 | 844.8, 216.0, 409.6, 43.2 | 2 |
| 6 | 54 | `universal.timing` | Universal | 0.66, 0.37, 0.32, 0.05 | 844.8, 266.4, 409.6, 36.0 | 1 |
| 7 | 57 | `universal.activity_trace` | Universal | 0.66, 0.43, 0.32, 0.10 | 844.8, 309.6, 409.6, 72.0 | 1 |
| 8 | 60 | `universal.secondary_trace` | Universal | 0.66, 0.54, 0.32, 0.10 | 844.8, 388.8, 409.6, 72.0 | 1 |
| 9 | 64 | `controls.scene_previous` | Controls | 0.02, 0.90, 0.08, 0.06 | 25.6, 648.0, 102.4, 43.2 | 1 |
| 10 | 66 | `controls.scene_next` | Controls | 0.12, 0.90, 0.08, 0.06 | 153.6, 648.0, 102.4, 43.2 | 1 |
| 11 | 68 | `controls.media_previous` | Controls | 0.24, 0.90, 0.08, 0.06 | 307.2, 648.0, 102.4, 43.2 | 1 |
| 12 | 70 | `controls.media_next` | Controls | 0.34, 0.90, 0.08, 0.06 | 435.2, 648.0, 102.4, 43.2 | 1 |
| 13 | 72 | `controls.reseed` | Controls | 0.46, 0.90, 0.10, 0.06 | 588.8, 648.0, 128.0, 43.2 | 1 |
| 14 | 81 | `flexible.primary` | Flexible | 0.66, 0.64, 0.32, 0.16 | 844.8, 460.8, 409.6, 115.2 | 1 |
| 15 | 86 | `flexible.secondary_a` | Flexible | 0.66, 0.81, 0.15, 0.06 | 844.8, 583.2, 192.0, 43.2 | 1 |
| 16 | 91 | `flexible.secondary_b` | Flexible | 0.83, 0.81, 0.15, 0.06 | 1062.4, 583.2, 192.0, 43.2 | 1 |
| 17 | 107 | `media_viewport` | **Universal** | 0.02, 0.16, 0.60, 0.70 | 25.6, 115.2, 768.0, 504.0 | 0 (no widgets) |
| 18 | 111 | `overlay.health` | Overlay | 0.02, 0.03, 0.20, 0.06 | 25.6, 21.6, 256.0, 43.2 | 1 |
| 19 | 119 | `overlay.transition` | Overlay | 0.02, 0.10, 0.20, 0.08 | 25.6, 72.0, 256.0, 57.6 | 3 |

The pixel values match the bundle's table and `hud-wireframe-bounds-v1.md:19-39` exactly.

The bundle does not mention one fact: `media_viewport` is registered with role **Universal** (`:107`), not a role of its own. It is therefore included in the Universal same-role overlap check, which it passes because it sits left of x = 0.62 and the Universal column starts at x = 0.66.

### 2.2 Count reconciliation: 19 vs 20 — `SOURCE VERIFIED` (static)

| Count | Value | Evidence |
|---|---|---|
| Declarations (`regions_.push_back(makeRegion(...))`) | **19** | `HudRegionCatalog.cpp:32-119` |
| Successful insertions | **19** | `std::vector::push_back`, unconditional, in a constructor with no branches. Nothing can fail or skip silently. |
| Unique IDs | **19** | All 19 literals are distinct. |
| Other writers of `regions_` | **0** | `regions_` is private (`.h:64`). The only other code touching it is read-only (`find`, `all`, `acceptsWidget`). A tree-wide search found no other `makeRegion` caller or mutator. |
| Loop at `:127-129` | adds 0 regions | It appends `BindingPlaceholder` to each existing region's accepted widget types. |

**Reconciliation:** the "20 IDs" claim is **this agent's own error** in the 2026-10-08 probe report (an off-repo document). Its phrase "19 bound IDs plus `media_viewport`" was also wrong; the correct statement is 18 + `media_viewport` = 19.

The only tracked count statement is `docs/reviews/hud-runtime-architecture-closure-review.md:167`: "bounds for all 19 regions". It is correct. No tracked document claims 20. **No twentieth region exists.**

**Runtime cardinality:** no existing test asserts `catalog.all().size()`. `test_region_bounds_within_canvas_and_no_same_role_overlap` (`hud_presentation_tests.cpp:891`) iterates every region, but its check count is not itemized per test in any report. Runtime proof would require building and running a test, which this read-only pass did not do. The static proof above is unambiguous; a runtime size assertion is a possible later authorized step (§9).

### 2.3 Sub-region slices are not catalog IDs — `SOURCE VERIFIED`
- Slicing is computed at draw time: `HudWireframeRenderer.cpp:405-412` divides a region's height evenly by the number of bindings drawn there.
- The slices have no IDs and no catalog entries.
- The documented slices are draw-time results for the current profiles, not catalog definitions:
  - `effect_summary`: 2 × 21.6 px;
  - `overlay.transition`: 3 × 19.2 px (`hud-wireframe-bounds-v1.md:41-46`).

### 2.4 Production renders 13 of the 19 regions — `SOURCE VERIFIED` (new finding)

**Production is fixed to the minimal slice:**
- `HudWireframeRenderer::renderScope_` defaults to `RenderScope::MinimalSlice` (`HudWireframeRenderer.h:197`).
- Production never changes it. `HudCompositorBridge.cpp:3-14` calls only `setup(1280,720)` and `setStudioMode(false)`.
- The only `setRenderScope` callers anywhere are in Validation Studio (`sketches/hud_validation_studio/src/ofApp.cpp:581, 733`).

**What is drawn in production:**
- The widget loop skips every binding outside `minimalSliceRegions()` (`HudWireframeRenderer.cpp:33-41, 388, 395`).
- `media_viewport` is drawn regardless (`:365-370`).
- Result: **12 binding regions + `media_viewport` = 13 regions drawn.**

**Six catalog regions are never drawn in production:**
- `universal.effect_summary`, `universal.timing`, `universal.activity_trace`, `universal.secondary_trace`;
- `flexible.secondary_a`, `flexible.secondary_b`.

**How this relates to accepted evidence:**
- `hud-runtime-architecture-closure-review.md:408-418` (§23) records that Studio was switched to `FullProfile` for the effects screenshot batch, because under `MinimalSlice` "none of the effects content would have been visible".
- `hud-canonical-1280-screenshot-index.md:19-24` records the same split: 13 baselines in `MinimalSlice`, 22 real-frame cases in `FullProfile`.
- So the accepted visual evidence for effects, timing, traces and secondary regions comes from **Studio `FullProfile`, not the production path**.
- The geometry of all 19 regions is accepted. Whether the six are meant to be *visible in production* is **not evidenced** in any acceptance record.

**This is not a source defect claim.** It is a discrepancy between the catalog (19), the production render set (13) and the DEC-015 effect-summary work (`HudRegionCatalog.cpp:44-50`). It needs an explicit Architecture statement before a Blueprint freeze (§7, item 2). Nothing was changed.

### 2.5 Projection, canvas, bounds enforcement, safe areas
- **Canvas** — `SOURCE VERIFIED`. The 1280×720 values appear in:
  - `HudCompositorBridge.cpp:6` (`renderer_.setup(1280.0f, 720.0f)`);
  - `HudWireframeRenderer.h:199-200` (defaults);
  - `ExperienceRuntime.cpp:90` (`services.canvasSize = glm::ivec2(1280, 720)`);
  - `sketches/experience_runtime/src/main.cpp:20` (window size).
- **No scaling transform** — `SOURCE VERIFIED`. The HUD draws at (0, 0)–(1280, 720) (`HudCompositorBridge.h:28-35`). `ToggleFullscreen` → `ofToggleFullscreen()` exists (`ExperienceRuntime.cpp:385-387`). Behavior at other display sizes is `NOT EVIDENCED`.
- **Bounds enforcement** — `SOURCE VERIFIED` (tests exist; not run here). Region-level enforcement exists only in tests:
  - within-canvas and same-role no-overlap (`hud_presentation_tests.cpp:891`; Overlay role exempt at `:915`);
  - exact secondary bounds (`:936`);
  - the truncation-budget fuzz (`:989`).
- **No runtime check:** `pixelBoundsFor()` performs no clamping or validation.
- **Safe areas** — `SOURCE VERIFIED` absent as data. There is no safe-area type, field, constant or table in `shared/src/hud-compositor`. Separation exists only as gaps between region rectangles. The bounds doc lists some of them: 25.6 px between secondary A/B, and 7.2 px from the secondary row to `flexible.primary` (`hud-wireframe-bounds-v1.md:67-71`). DEC-012 names "safe areas" as Blueprint-invariant without defining them.

---

## 3. Bounds and typography acceptance

### 3.1 What the documents themselves say — `DOCUMENT ASSERTED`
- `docs/reviews/hud-wireframe-bounds-v1.md:3-8` (`d25cc13`): "Status: Architecture-Closure Session deliverable … this document records it precisely, it does not redesign it."
- `docs/reviews/hud-typography-metrics-v1.md:3-8` (`d25cc13`): "Status: Architecture-Closure Session deliverable … closes wireframe typography acceptance without freezing final skin typography."
- **Neither document contains an acceptance statement, approver, or approval date.**

### 3.2 The acceptance chain — `DOCUMENT ACCEPTED`, indirect
1. `hud-runtime-architecture-closure-review.md:571-575` (§30): "**READY FOR ARCHITECTURE ACCEPTANCE**". §31 (`:577-607`) ends with "**Final disposition: Ready for HUD Runtime acceptance.**" Both are recommendations, not grants.
2. `Architecture-Authorization-Temporal-Production-Scene-2.md:22` (Architecture-owned, `d25cc13`, undated): "- HUD Runtime infrastructure accepted." This is a single bullet. **It does not name the bounds or typography documents.**
3. `03-work-registry.md` HUD-001 is **ACCEPTED**. It cites the closure review §30–31, the final narrow closure patch report (1307/1307), and item 2 above. Source State: `COMMITTED (d25cc13; pushed)`.
4. DEC-017 (`Decision-Log-v3.md:146-154`) is Approved. Its follow-up reads "Joint HUD/ExperienceRuntime acceptance test before wireframe approval." No tracked record shows "wireframe approval" as a separate act beyond item 2.

**Conclusion:** the geometry and typography are accepted **as part of HUD-001 "HUD Runtime infrastructure"**. There is `NOT EVIDENCED` any explicit Architecture approval naming `hud-wireframe-bounds-v1.md` or `hud-typography-metrics-v1.md`, any approver, or any approval date on the documents themselves. The file revision of both is `d25cc13`, unchanged since.

### 3.3 Geometry and typography QA trace

**Geometry tests** — `SOURCE VERIFIED` present; results `AGENT-REPORTED`, not run here: `hud_presentation_tests.cpp:891`, `:936`, `:989`. Reported totals: 676/676 dependency-free and 1307/1307 overall (`hud-final-narrow-closure-patch-report.md:185, 191`).

**Typography facts** — `SOURCE VERIFIED`:
- **Font:** the openFrameworks built-in bitmap font. Drawing goes through `ofDrawBitmapString` only (`HudWidgetDrawUtils.h:84-90`), with no `ofTrueTypeFont` anywhere in `widgets/`.
- **Advance:** `HudTextMetricsCache::kGlyphWidthLocal = 8.0f` (`HudTextMetricsCache.h:51`), with width = 8 × length (`.cpp:8`).
- **No scaling:** `drawText()` ignores `scale` (`(void)scale;`, `HudWidgetDrawUtils.h:85`). There is no `ofScale` in drawing code; the only mentions are comments.
- **Ellipsis:** ASCII `"..."` (`HudTextMetricsCache.cpp:25`). If even that doesn't fit, the result is empty (`:28`). The cache is keyed by text plus the rounded budget (`.h:66-75`).
- **The 13 px cell height, cap height, ascender, descender and baseline** are document-asserted only. The document itself says cap height, ascender, descender and baseline were not measured (`hud-typography-metrics-v1.md:65-69`).

**Typography tests** — `SOURCE VERIFIED` present; results `AGENT-REPORTED`: `hud_typography_tests.cpp` has 4 test functions (`:32, 48, 56, 72`); the document reports 14 checks.

**`universal.timing` gap** — still present, `SOURCE VERIFIED`:
- `hud-wireframe-bounds-v1.md:26` shows its inset as "not yet audited".
- `hud-typography-metrics-v1.md:89-106` omits it from the per-region table.
- The region *is* bound (`HudPresentationProfile.cpp:95`), but by §2.4 it is **not drawn in production**, so the gap does not affect production rendering today. It is a documentation gap.

**Status:** the wireframe typography is accepted; final skin typography is explicitly out of scope (`hud-typography-metrics-v1.md:6-8, 50-56`).

---

## 4. MediaViewportMesh — `SOURCE VERIFIED` (static); visual proof `AGENT-REPORTED`

- **Call site:** `HudWireframeRenderer.cpp:365-370`. It runs first and on every draw, regardless of render scope, with `params.bounds = pixelBoundsFor("media_viewport")` (pixels 25.6, 115.2, 768.0, 504.0).
- **Shape:** a triangle fan from the bounds center (`MediaViewportMesh.cpp:65-145`):
  - rounded top-left, top-right and bottom-left corners (arcs at `:123, 127, 136`);
  - a 45° straight cut at the lower right (`:129-132`).
- **Default geometry:** `cornerRadius 18`, `bevelSize 42`, `degreesPerArcStep 6` (`MediaViewportMesh.h:47-54`). Both radius and bevel are clamped to ≤ 40 % of the shorter side (`.cpp:75-76`); the clamps are inactive at 768×504.
- **UVs:** cover-fit (`coverUV`, `.cpp:27-49`), baked at rebuild time. At draw, they are converted to the bound texture's native coordinates via `getCoordFromPercent` and restored afterwards (`:181-192`).
- **Rebuild:** only when params or the source size change, by exact comparison (`:53-63`).
- **Clipping mechanism:** **geometry only.** There is no stencil, scissor, shader-discard or raster-mask path in `shared/src/hud-compositor/` (search: `stencil|discard|glScissor|mask`; the only hits are header comments explaining why those were rejected, `MediaViewportMesh.h:18-22`).
- **Blending:** `ofEnableAlphaBlending()` (straight-alpha blend, `.cpp:151`). An untextured mesh draws the placeholder color rgb(20, 26, 24).
- **Tests:** present, not run — `hud_media_viewport_tests.cpp` has 7 test functions (`:49, 71, 93, 110, 139, 162, 180`).
- **Visual and GL proof:** `AGENT-REPORTED` only (Studio screenshots; the closure review notes an ARB-UV bug was found and fixed). No GL run was done in this pass.

---

## 5. Blueprint identity search

**Repository** (`git grep`, 950 tracked files):
- Pattern: `blueprintId|blueprintVersion|hud-blueprint-v1|HUD-Blueprint-v1|Blueprint v1|canonical-v1|blueprint freeze|Blueprint package`, case-insensitive, with `blueprint_emergence`/`blueprint-emergence` scene hits filtered out.
- Run once over the repo excluding `tools/` and `docs/reviews/logs/`, and once over `tools/` separately.

| Kind | Hits | Classification |
|---|---|---|
| (a) Accepted Blueprint **policy** | DEC-012 (`Decision-Log-v3.md:84-88`): one canonical Blueprint, same geometry, region IDs, safe areas, layer order and widget layout. It has no Status field, ID or version. Scene/HUD Contract v1 §12 (`:295-319`) names an asset folder `assets/hud/layouts/canonical-v1/`, which is a directory convention and does not exist on disk. | `DOCUMENT ACCEPTED` (policy only) |
| (b) Proposed or example identifiers | Roadmap deliverable `HUD-Blueprint-v1.md` and checklist item (`Ecopunk-HUD-System-Master-Roadmap.md:973, 1502`), unproduced. Loader draft `hud-blueprint-v1` / `1` (off-repo). In `tools/`: only the test-only identity `test-fixture-blueprint-not-canonical`, plus the draft placeholder used in a test that proves it is *not* accepted by default. | `DRAFT` / test-only |
| (c) An Architecture-approved canonical ID/version and freeze record | **None.** Explicit negatives: `hud-runtime-architecture-closure-review.md:54-57` ("HUD Blueprint Package v1 … no such document exists"), `:568` ("No Blueprint freeze claimed"); `03-work-registry.md:632` (HUD-006 BLOCKED); `:1286` (Blueprint freeze gated on Pi budgets, per an off-repo restart audit). | `NOT EVIDENCED` |

**Source constants:** no `blueprint`-identity constant exists in `shared/src/hud-compositor`, `hud-runtime`, `scene`, or the ExperienceRuntime/bridge files. The hits are the Blueprint *scene* profile and vocabulary (`HudPresentationProfile.cpp:246-269`, `HudVocabularyResolver.cpp:48, 114-116`) and the stale placeholder comment at `HudRegionCatalog.h:9`.

**Off-repo** (`~/Downloads`, read-only):
- Files checked:
  - `HUD-006-Blueprint-Freeze-Readiness-Report.md` (HUD Art);
  - `HUD-006-Canonical-Blueprint-Closure-Evidence.md` and its byte-identical `(1)` copy (HUD Art);
  - `HUD-006-HUD-004-Primary-Evidence-Bundle.md` (HUD Runtime manager).
- Every one states that the canonical ID and version are "Not Evidenced" or "Missing". None proposes a literal value.

**Result:** a canonical Blueprint pair is **not evidenced in the searched repository or the off-repo files listed**. That is not proof that no such record exists anywhere.

**HUD-006 closure blockers:**
1. An Architecture identity/version/compatibility record. It is missing.
2. An explicit acceptance naming the accepted geometry and typography documents and revisions (§3.2). Today acceptance is indirect only.
3. A safe-area decision: explicit data contract, or the documented geometric gaps (§2.5).
4. The production render-set statement for the six regions that are never drawn (§2.4).
5. Reconciliation of historical illustrative IDs and runtime-mask language in the preproduction package. That package is not on disk, so this could not be verified.
6. Architecture's stated position on whether Pi budgets gate the Blueprint freeze. The registry (`:1286`) cites an off-repo audit saying they do.

**Compatibility recommendation (nonbinding):**
- `blueprintId` + `blueprintVersion` should identify the frozen layout/region/semantic contract: the 19-region catalog geometry, typography, viewport mesh and render set.
- `skinId` + `skinVersion` independently identify an authored package and its release.
- A skin release must not change its Blueprint target silently.
- Until HUD-006 closes, the expected Blueprint pair stays an explicit caller input that fails closed (`SKIN_POLICY_BLUEPRINT_UNSET`, already the accepted foundation behavior).

---

## 6. HUD-004 clause matrix

**Sources:**
- **Foundation:** `tools/hud-skin-manifest/` at `4bc1be7`. Tests `SOURCE VERIFIED` present; results `AGENT-REPORTED` 394/394, not run here.
- **Draft:** `~/Downloads/skin-package-loader-contract-v1-first-draft.md` (`DRAFT`, untracked).
- **Production:** HEAD.
- **Rulings:** `Architecture-Acceptance-HUD-Skin-Manifest-Final-Schema-Alignment.md` (`4bc1be7`) plus the off-repo rulings it cites.

**Production search** (`git grep` over `shared/` and `sketches/` for `HudServices|HudAssetPaths|assets/hud|HudSkin|SkinPackage|validateSkinPackage`): **0 hits**. There is also no image loading (`ofImage|ofLoadImage|loadImage`) and no blend-mode code in `shared/src/hud-compositor`.

**Build reachability:** `SOURCE VERIFIED`, unchanged by this pass.
- `experience_runtime` compiles HUD `.cpp` files only through `src/` symlinks (e.g. `src/HudRegionCatalog.cpp -> ../../../shared/src/hud-compositor/HudRegionCatalog.cpp`). Its only external source path is `shared/src/video-playback` (`config.make:42`).
- Validation Studio sweeps `shared/src/hud-compositor` (`config.make:18`).
- No symlink points into `tools/`.

| Clause | Foundation (accepted, tools-only) | Draft | Production (HEAD) | Status by claim |
|---|---|---|---|---|
| Canonical canvas / logical dimensions | 1280×720 required on every declared shell, decoration and emissive asset (`HudSkinManifest.cpp:343-344`, `.h:160-161`) | Same canvas; physical size Pi-dependent | HUD renders at a fixed 1280×720 (§2.5) | Logical rule **ACCEPTED**; scaling to other outputs **NOT EVIDENCED** |
| Blueprint compatibility | Explicit caller pair, fail closed (`:341`) | Example `hud-blueprint-v1`/1 | None | Rule **ACCEPTED**; example **DRAFT**; canonical pair **MISSING** (HUD-006) |
| `skinId` / `skinVersion` | Slug, consecutive internal hyphens allowed (`:96`); canonical uint32 components (`:111-140`) | Proposed fields | None | **ACCEPTED** (tools); production parser **MISSING** |
| JSON strictness / unknown keys / duplicates | Rejected (`:353`, `:456`, `:263`) | Unknown top-level "may be ignored" | None | **ACCEPTED**; the draft's lenient top-level rule is **SUPERSEDED** |
| Shell / decoration / emissive requiredness | Shell required, others optional, conflicting flags rejected (`:292`) | Same | None | **ACCEPTED** manifest subset; image content **NOT EVIDENCED** |
| Path safety | All `..`, all symlinks, backslash, absolute, URI/drive, control characters rejected | Escape-only rules | None | Strict rule **ACCEPTED**; weaker draft rule **SUPERSEDED** |
| Runtime raster media / panel masks | Mask classes rejected as unknown | Runtime masks excluded | `MediaViewportMesh` geometry clipping; no mask path (§4) | **ACCEPTED** (mesh is the authority); Roadmap Phase 11 `media-mask.png` / `panel-mask.png` as runtime assets **SUPERSEDED**; authoring-only masks need HUD Art clarification |
| Straight-alpha PNG | No decoding (fixture `shell.png` is 0 bytes) | Straight alpha proposed | No skin decoding or blending; viewport uses straight alpha blending (`MediaViewportMesh.cpp:151`) | Interchange convention: see note 1; blending of skin layers **NOT EVIDENCED** |
| Atlas / icon roles | Rejected, temporarily (`:487`) | Paired image + metadata; runtime icon roles assumed | No icon-role catalog (Icon Strip deferred, `HudWidgetTypes.h:25-32`) | Future direction **ACCEPTED**; schema **DRAFT**; consumption and role catalog **MISSING** |
| One setup-selected skin, no live switching | Not applicable (no loader) | Proposed | No selection or switching code | Design **DRAFT** (note 2); implementation **MISSING** |
| Validation vs image decoding | Manifest and file existence only | Staged, including decode | None | Manifest subset **ACCEPTED**; image correctness **NOT EVIDENCED** |
| Atomic activation / wireframe fallback | Not applicable | Proposed | The wireframe always renders; no skin-failure path | Design **DRAFT** (note 2); runtime semantics **MISSING** |
| GPU residency / teardown | Not applicable | Proposed lifecycle | None | **DRAFT**; **MISSING**; performance **NOT EVIDENCED** |
| Runtime/Studio loader parity | Tool reachable by neither build | Same loader in both | Studio shares the renderer only (DEC-017) | Design **DRAFT** (note 2); parity **MISSING**; the build-reachability risk stands |
| §12 `HudServices` / `HudAssetPaths` asset root | Not applicable | Package root supplied by runtime | 0 symbols; `assets/` has only `shared/` | Contract text **ACCEPTED** (frozen Scene/HUD Contract v1); implementation **MISSING** (joint ExperienceRuntime/HUD gate) |
| Pi ceilings | None (by design) | Named parameters, no values | No measurement (PI-003 BLOCKED) | **NOT EVIDENCED** |

**Notes:**
1. The bundle classifies straight alpha as "ACCEPTED PROVISIONALLY", and one-skin, fallback and parity as "Design ACCEPTED". **No tracked record and no on-disk Architecture document states those acceptances.** The foundation rulings document addresses only manifest-schema items. Off-repo HUD Art reports list these items as "Proposals — not Architecture rulings". This report therefore classifies them **DRAFT** unless the bundle's source ruling is supplied. The difference is preserved here, not resolved.
2. The 394/394 manifest tests are not evidence for image decoding, GPU behavior, Studio parity, fallback or Pi behavior.

---

## 7. Architecture decisions requested

**HUD-006 freeze prerequisites:**
1. **Record the canonical region inventory as 19.** This report's §2.1 table is a candidate source, and it matches `hud-wireframe-bounds-v1.md`. Record "20" as an erratum of the 2026-10-08 probe report. No new IDs.
2. **Production render set.** State whether the Blueprint's runtime contract is all 19 regions visible in production, or the current 13 (`MinimalSlice`). If 19, a separate authorized change is needed; nothing was changed here.
3. **Explicit geometry and typography acceptance.** Name `hud-wireframe-bounds-v1.md` and `hud-typography-metrics-v1.md` at revision `d25cc13` as the accepted Blueprint geometry and typography, or state that HUD-001's "HUD Runtime infrastructure accepted" suffices. Include the `universal.timing` documentation gap.
4. **Safe areas.** Decide whether they are an explicit data contract or the existing geometric gaps (`hud-wireframe-bounds-v1.md:67-71`).
5. **Identity record.** Issue the canonical `blueprintId` / `blueprintVersion`, the revision they pin, and the compatibility and version-bump rule. This agent proposes no literal.
6. **Pi boundary.** State whether Pi budgets gate the Blueprint identity/geometry freeze or only skin production acceptance.

**Later HUD-004 / Pi decisions (not HUD-006 prerequisites):**
7. Supply or confirm the rulings the bundle cites for straight alpha, one active skin, atomic fallback and Studio parity (§6, note 1).
8. Retire runtime mask exports from the Roadmap and preproduction guidance (HUD Art), and keep atlas and icon roles deferred.
9. Only after HUD-006 closes: a joint HUD Runtime/ExperienceRuntime authorization for §12 asset-root delivery and loader planning. Production and Studio build reachability must be handled in the same change.

---

## 8. Verification and hygiene

**Commands run:** read-only only —
- `git rev-parse`, `branch`, `status`, `log -1 -- <path>`, `diff --stat 61a4acf HEAD -- …`, `show --stat`, `branch -r --contains`, `grep`, `ls-files`;
- `grep`, `awk`, `sed`, `ls`, `cmp`, `head` over repository and `~/Downloads` files;
- one `python3` stdin computation reproducing float32 `normalized × canvas` (no files written).

**Tests:** **none run, none built.** Every test result cited is `AGENT-REPORTED`.

**Modifications:** none. `git status --short` was clean before and after. Nothing was staged, committed or pushed. This report was written only to the session scratchpad, outside the repository.

---

## 9. Next gated increment (recommendation only; not authorized)

1. **Architecture rulings** (§7 items 1–6), especially the render-set statement and the identity record. These are documents, not code.
2. **Then, if Architecture wants runtime proof of cardinality and render set:** a small, separately authorized test-only change.
   - Add an assertion that `HudRegionCatalog().all().size() == 19` with the exact ID list.
   - Add an assertion of the production `RenderScope`.
   - Both go in `hud_presentation_tests.cpp`, with no source change.
3. **Separately:** documentation-only corrections, under their own authorization:
   - the stale "PLACEHOLDER" comment (`HudRegionCatalog.h:6-12, 48`);
   - the "consumes FakeHudFrameData / no relationship to ExperienceRuntime" comment (`HudWireframeRenderer.h:8-15`);
   - the `universal.timing` audit gap.

No production integration is implied.
