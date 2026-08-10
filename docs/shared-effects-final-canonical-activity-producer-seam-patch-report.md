# Shared Effects — Final Canonical Activity Producer Seam Patch Report

**Status:** Implemented, verified by a real build + real captured console output. Not yet Architecture-reviewed — same status class as every other Shared Effects document in this chain.

---

## 1. Exact production owner identified

**Namespace/class:** `TFEffectPicker` (global namespace, not `videoeffects` — it's a scene-owned class, not part of the Shared Effects library itself)
**Header/source:** `sketches/temporal-fields/src/TFEffectPicker.{h,cpp}`
**Lifetime/owner:** owned by value inside `TFBackgroundLayer` (`sketches/temporal-fields/src/TFBackgroundLayer.h`), which is itself owned by value inside `ofApp` for the `temporal-fields` sketch. Constructed at sketch startup, lives for the sketch's whole process lifetime — there is no separate setup/teardown cycle beyond the sketch's own.
**Which production path instantiates/uses it:** `temporal-fields/src/ofApp.cpp` → `backgroundLayer.setup(...)` → `TFEffectPicker::setup(ShaderLibrary*)`. Every frame, `TFBackgroundLayer::draw()` calls `effectPicker.drawCurrent(...)`, which is the actual code path that renders the currently-selected shared effect onto real video content. This is genuine, real, end-user-visible production rendering — not a fixture, not tooling.

**`videoeffects::VideoEffectService` is explicitly NOT the production owner.** Confirmed by a full-repository `grep` sweep of every non-generated (`.h`/`.cpp`, not `.d` dependency-file) reference:
- `sketches/shader-effect-debugger/src/ofApp.{h,cpp}` — developer tooling, excluded from production per `DEC-003`.
- `sketches/experience_runtime/src/FakeScene.{h,cpp}` — an explicit test/demo fixture. Its own header comment states directly: *"Health is derived via the REAL `videoeffects::deriveEffectHealth()` against a real (honestly empty — this scene requests zero shader effects) `VideoEffectLoadReport`, never synthesized directly — see the .cpp for why a full `VideoEffectService` was NOT instantiated here."* — i.e., a different, concurrent session already investigated this exact question for `ExperienceRuntime`'s fixture and independently reached the same conclusion this report reaches: `VideoEffectService` is not wired into any real production render path today.
- No other file in `shared/` or `sketches/*/src/` (excluding `obj/`, which are build artifacts, not source) references `VideoEffectService` at all.

**`blob-region-prototype` was also checked and ruled out** (see `docs/blob-region-prototype-shared-catalog-migration-status.md`, prior session): it consumes the canonical catalog for uniform *defaults* via `shared/src/VideoRegionEffectRenderer.cpp`, but its "which effect is currently selected" state is two independent local `ofParameter<int>` GUI sliders living directly in `ofApp.cpp` — not encapsulated in any Shared-Effects-owned class at all. There is no object to attach an accessor to for Blob today; this is unchanged by this patch and not addressed here (see §7).

---

## 2. Exact accessor implemented

**Declaration:** `sketches/temporal-fields/src/TFEffectPicker.h`
```cpp
videoeffects::EffectActivityStatus activityStatus() const;
```
**Definition:** `sketches/temporal-fields/src/TFEffectPicker.cpp`, `TFEffectPicker::activityStatus()`.

This accessor **already existed**, added during the prior Shared Effect Knowledge Engineering Session 2 (the first real `temporal-fields` integration). This patch's actual change is narrow: it adds real **health derivation** to that existing accessor (previously `health` silently defaulted to `Ready` always, never actually consulting any owned state) — see §3's `health` row.

Per §3/§4 of the task ("Use the repository's established naming convention... Do not add both"): `activityStatus()` is the name already established by the existing accessor; no second name (`effectActivityStatus()`) was introduced.

---

## 3. Evidence it owns real production effect state

Production path: `ofApp::update()`/`draw()` → `TFBackgroundLayer::update()`/`draw()` → `TFEffectPicker::update()` (advances `timer`, calls `pickNext()` on the configured cadence) → `TFEffectPicker::drawCurrent()` (the real rendering call, uses `currentEffect`/`paramX..W`/`shaderLib`).

| Snapshot field | Real source |
|---|---|
| `health` | `shaderLib->has(currentEffect)` — the exact same condition `drawCurrent()` itself checks (line-for-line the same boolean) to decide whether to run the shader or fall back to a raw, unshaded draw. This is genuinely the only failure mode this class has, since it owns no `VideoEffectLoadReport` (it never uses `VideoEffectService`, only the lower-level `VideoEffectRegistry` for parameter defaults + its own `ShaderLibrary` for rendering). Newly added by this patch — previously unconsulted. |
| `messageId` | Set to the stable id `"effect.shader_unavailable"` only when the above condition is true; `nullopt` whenever `health == Ready` (enforced by the same `if` branch, not a separate check that could drift out of sync) |
| `slotId` | Literal `"temporal_fields.background"` — this class only ever drives one content layer, so there is exactly one possible slot; unchanged by this patch |
| `effectId` | `currentEffect` — the exact `std::string` `pickNext()` assigned via `tfWeightedPick()` against the real registered `ShaderLibrary`/catalog names, the same value `drawCurrent()` uses to select which shader to bind; unchanged by this patch |
| `displayName` | `tfCatalogRegistry().getDefinition(currentEffect)->displayName`, falling back to `currentEffect` itself if the id isn't in the (single-pass-only) catalog registry subset this class checks against; unchanged by this patch |
| `phase` | Hard-coded `EvolutionPhase::Holding` — **honest, not a placeholder**: this picker hard-cuts between effects on its own timer (`pickNext()` simply reassigns `currentEffect`), it never blends/transitions the way `EffectEvolutionController` does, so `Holding` (not `Transitioning`) is the true instantaneous state, always; unchanged by this patch |
| `transitionProgress01` | Hard-coded `1.0f`, for the same reason as `phase` — there is no partial-transition state to report; unchanged by this patch |
| `prominence` | Hard-coded `1.0f` — this class drives the sole content layer in `TFBackgroundLayer`'s `FULL_VIDEO` mode, so there is no competing slot to rank against and no meaningful lower value; unchanged by this patch |

**Slots-empty case:** `currentEffect.empty()` (the "Raw / No Effect" weighted-pick outcome) returns a status with zero slots and `health = Ready` — a real, reachable, genuinely common production state (this class's own `weights.rawWeight` makes "Raw" a real, frequently-selected outcome in normal operation), not a fabricated edge case.

---

## 4. Files changed

| File | Reason |
|---|---|
| `sketches/temporal-fields/src/TFEffectPicker.cpp` | Added real `health`/`messageId` derivation to the existing `activityStatus()`, from `shaderLib->has(currentEffect)` — the only change to production logic in this patch |
| `sketches/temporal-fields/src/TFActivityStatusSelfTest.{h,cpp}` | **New.** Focused test proving the accessor contract (§6 below) against the real compiled class |
| `sketches/temporal-fields/src/ofApp.cpp` | Wired the self-test to run automatically at `setup()`, right after `shaderLib.setup()` — using its own scratch `TFEffectPicker` instances against the real, already-loaded `ShaderLibrary`, never touching the scene's own live `backgroundLayer`/`effectPicker` |

No file under `ExperienceRuntime`, `HudFrameData`, `SceneManager`, `FakeScene`, `IEcopunkScene`, `SceneHudStatus`, or any Shared Effect Knowledge frozen public struct was touched — confirmed by `git diff --stat` before writing this report.

---

## 5. Tests/builds

```
cd shared/src/video-effects/test && make -f Makefile.tests clean && make -f Makefile.tests test
  → 93/93 checks passed

python3 scripts/check-video-effect-drift.py
  → All video-effect drift checks passed (22 canonical ids, no duplicates; 9 pre-existing
    advisory NOTE lines, none new from this patch)

cd sketches/temporal-fields && make Release -j1
  → compiling done; Mach-O 64-bit executable produced

./bin/temporal-fields.app/Contents/MacOS/temporal-fields   (real launch, 8s, console captured)
  → [notice] TFActivityStatusSelfTest: PASS: 25/25 checks passed
```

`TFActivityStatusSelfTest`'s 25 checks cover three deterministic, forced scenarios against the real `TFEffectPicker` class (forcing outcomes via `Weights` with a zero/near-infinite weight split — confirmed deterministic by reading `tfWeightedPick()`'s own cumulative-weight implementation, not assumed): forced Raw (present-empty), forced known-good real effect (`"desaturate"`, confirmed both loaded in the real `ShaderLibrary` and present in the real catalog registry), and forced deliberately-unregistered name (Degraded health).

---

## 6. Side-effect proof

- **Repeated reads are stable:** every one of the three test cases calls `activityStatus()` twice with no `update()` between calls and asserts full structural equality (`schemaVersion`, `health`, `messageId`, every slot's `slotId`/`effectId`/`displayName`/`phase`/`transitionProgress01`/`prominence`) — proven, not assumed, for all three health outcomes (Ready-empty, Ready-active, Degraded).
- **Selection state unchanged by reads:** `getCurrentEffectName()` (the only public window into `currentEffect`) is captured before and after each `activityStatus()` call in every case and asserted identical.
- **History/cooldown/evolution:** this class owns none of these concepts at all (confirmed by direct inspection — no history buffer, no cooldown timer, no `EffectEvolutionController` member exists anywhere in `TFEffectPicker`), so there is nothing of that kind for a read to disturb; the claim rests on the class's actual member list (`shaderLib`, `weights`, `timer`, `currentEffect`, `knowledgeBase`, `paramX..W`, two FBOs), all inspectable in the header, none touched by `activityStatus()`'s body (it is `const`, contains zero assignment statements to any member — verifiable directly from the diff).
- **No file I/O:** `activityStatus()` calls only `shaderLib->has()` (in-memory map lookup) and `tfCatalogRegistry().getDefinition()` (in-memory map lookup against a `static` registry built once at process start) — no `ofFile`/`ofJson`/disk access of any kind.
- **No dirty-flag/event consumption:** confirmed by inspection — no flag or queue of any kind is read or cleared in this method.

---

## 7. Remaining risks

- **Implementation:** none identified beyond what's already documented in `docs/temporal-fields-knowledge-pack-integration.md` (unchanged by this patch).
- **Migration:** `temporal-fields` itself is not migrated to `IEcopunkScene` yet, so this accessor has no live `SceneManager`/`ExperienceRuntime` caller today — it is proven correct and ready, but currently only reachable by the self-test and by a future scene adapter. This is expected and matches the roadmap's own sequencing (scene migration is explicitly out of this patch's scope).
- **Ownership (real, not hypothetical):** `blob-region-prototype` has no equivalent owner at all (§1) — if/when Blob needs to participate in `HudFrameData.effects`, this same "add an accessor to an existing owner" pattern does not directly apply, because no such owner currently exists for Blob. That is a real gap, but not one this narrow patch was scoped to close (per the task's own stop condition, applied at the per-scene level: `TFEffectPicker` genuinely exists and qualifies, so this patch proceeded for `temporal-fields`; Blob's absence of an owner is reported, not silently worked around).
- **`EffectHealth::Failed` is unreached by this owner** — `TFEffectPicker` has no hard-failure concept (no `VideoEffectLoadReport`), so only `Ready`/`Degraded` are honestly reachable through it. Not fabricated to satisfy a completeness checklist; noted directly in `TFActivityStatusSelfTest.cpp`'s own comment.

---

## 8. Explicit handoff to ExperienceRuntime

**Source domain:** Shared Effects and Shader Debugger
**Target domain:** ExperienceRuntime and SceneManager

**Decision or finding:** `TFEffectPicker::activityStatus()` (`sketches/temporal-fields/src/TFEffectPicker.h`) is a real, production-owned, side-effect-free, const `EffectActivityStatus` snapshot accessor, verified against the real compiled class. `videoeffects::VideoEffectService` is confirmed **not** production-authoritative (debugger + fixture-only usage, verified by full-repo grep). No second effect-activity authority exists.

**Evidence:** §1–§6 above; real build + real captured console output (`TFActivityStatusSelfTest: PASS: 25/25 checks passed`); `EffectSceneCompatibility`/`EffectActivityStatus` unit suite (93/93); drift checker clean.

**Required action:** once `temporal-fields` (or any future scene) is wrapped in an `IEcopunkScene` adapter, that adapter should call `TFEffectPicker::activityStatus()` (via whatever object then owns the `TFEffectPicker` instance) and expose it through whatever the scene-adapter layer's equivalent of `SceneManager::captureEffectActivityStatus()` is, exactly once per frame, feeding `HudFrameData.effects`. This report does not implement that adapter (explicitly out of scope, §"Do not include" in the task).

**Contract impact:** None beyond the already-approved `DEC-015` additive transport. No frozen struct changed.

**Dependencies:** `temporal-fields`'s own `IEcopunkScene` migration (roadmap Phase 8, not started).

**Acceptance criteria:** satisfied for the *owner+accessor* half of DEC-015's chain (this report); the *ExperienceRuntime queries it once per frame* half remains gated on scene migration, not on anything in Shared Effects' domain.

---

## 9. Final disposition

**Ready for ExperienceRuntime seam proof** — for `temporal-fields` specifically, once it has a scene adapter to call this accessor from. Not blocked on any further Shared Effects work.

**Requires Architecture ownership decision** — for `blob-region-prototype` and every other not-yet-integrated scene, since no existing Shared-Effects-owned object holds their active-effect state for an accessor to attach to (§1, §7). This is reported as a real, separate gap, not silently worked around by inventing a new cross-scene service.
