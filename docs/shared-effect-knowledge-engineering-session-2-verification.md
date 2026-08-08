# Shared Effect Knowledge — Engineering Session 2: Schema Verification and Debugger Validation

**Status:** Verification/hardening complete, **not yet Architecture-reviewed** — same status class as `docs/shared-effect-knowledge-scoped-extension.md` (Session 1), which this document extends rather than replaces. Treat both as proposals until Architecture signs off, per `docs/shared-project-docs/01-architecture-governance.md`'s "shared effect-knowledge design" review checkpoint.

**Note on source documents:** the brief for this session cited `Cross-Domain-Reconciliation-Engineering-Session-1.md` and `shared-effects-post-implementation-data-and-convergence-review.md` as authoritative. Neither file exists anywhere in this repository (confirmed by `find`/`git status` before starting). This document is grounded instead in the documents that *do* exist and were actually read: `HUD-Semantic-Slot-Model-v1.md`, `Scene-HUD-Contract-v1.md`, `01-architecture-governance.md`, `02-cross-domain-handoff-protocol.md`, and Session 1's own `shared-effect-knowledge-scoped-extension.md`. Flagged per the same discrepancy-reporting principle Session 1 used — the requested work itself was reasonable and is what follows.

---

## 1. Exact `EffectActivityStatus` schema

**Header:** `shared/src/video-effects/knowledge/EffectActivityStatus.h`
**Namespace:** `videoeffects`
**Depends on:** `EffectEvolutionController.h` (for `EvolutionPhase`) — no other video-effects or openFrameworks headers.

```cpp
struct EffectActivitySlot {
    std::string slotId;       // caller-defined, e.g. "primary", "quadrant_0"
    std::string effectId;     // canonical id, e.g. "heatmap_recolor"
    std::string displayName;  // curated label, e.g. VideoEffectDefinition::displayName
    EvolutionPhase phase = EvolutionPhase::Holding;   // Holding | Transitioning
    float transitionProgress01 = 1.0f;
    float prominence = 1.0f;  // caller-assigned dominance-ranking salience
};

struct EffectActivityStatus {
    uint32_t schemaVersion = 1;
    std::vector<EffectActivitySlot> slots;
};

struct DominantEffectResult {
    std::string effectId;
    bool transitioning = false;
    float transitionProgress01 = 1.0f;
    float prominence = 0.0f;  // merged (max of member slots')
};
```

- **Schema/version field:** yes, `EffectActivityStatus::schemaVersion` (default 1). Not currently enforced/checked anywhere — there is exactly one version and no (de)serialization of this type at all (it is constructed in-process by a scene, never written to or read from disk).
- **Contained types:** `EffectActivitySlot` (one per active effect instance/slot), `DominantEffectResult` (one per ranked group, output-only).
- **`EffectInstance` field:** none — this type does not reference `VideoEffectInstance` at all; it is a plain-data snapshot a caller populates from whatever instance state it already owns.
- **Effect ID representation:** `std::string effectId`, ordinary (non-interned, non-borrowed) `std::string`, expected to be a canonical `VideoEffectDefinition::id`. Not validated against the catalog by this type itself.
- **Preset ID representation:** **none exists.** There is no `presetId` field anywhere in this type or in `KnowledgeEntry` (§2) — see §6's effect-vs-preset table for why.
- **Channel/role representation:** `slotId` (free-form `std::string`, caller-defined) is the closest analog — e.g. `"quadrant_0"`. Not a typed enum, no reserved vocabulary.
- **Health representation:** **not represented.** No field maps to `HUD-Semantic-Slot-Model-v1.md` §12.6's `effects.health`. See §7, Question 1.
- **Transition representation:** `EffectActivitySlot::phase` (reused `EvolutionPhase` — see §5) + `transitionProgress01`.
- **Dominance representation:** not stored on the type itself — computed on demand by `resolveDominantEffectIds()`/`resolveDominantEffectLabels()` (free functions, §4), which return `std::vector<DominantEffectResult>`/`std::vector<std::string>` respectively, ordered most-dominant-first.
- **Contribution/intensity representation:** `EffectActivitySlot::prominence` (caller-assigned float, no fixed scale/unit beyond "higher = more dominant"). This is the same field dominance resolution ranks on — see §7, Question 1's caveat on reusing it for `effects.intensity`.
- **Collection type/ordering:** `std::vector<EffectActivitySlot>`, insertion order as populated by the caller; no ordering guarantee until passed through `resolveDominantEffectIds`/`Labels`, which impose a defined, deterministic order (§4).
- **Maximum/capacity assumptions:** none enforced by the type; `DominanceConfig::maxLabels` (default 2) caps the *output* of resolution, not the input `slots` vector's size. `HUD-Semantic-Slot-Model-v1.md` §20 recommends a cap of 8 active effect IDs — not enforced in code, a caller-side responsibility.
- **Default construction:** `EffectActivityStatus{}` has `schemaVersion = 1`, `slots` empty. An empty-slots status is the correct representation of "no effect currently active" (see §3's "Raw / No Effect" handling in the temporal-fields integration).
- **Missing vs. empty:** `slots.empty()` is the only "missing" signal — there is no separate `std::optional<EffectActivityStatus>` wrapper defined by this type itself (a consuming struct, e.g. a future `HudFrameData`, would need to add that optionality itself, per `HUD-Semantic-Slot-Model-v1.md` §10's `std::optional<EffectActivityStatus> effects`).
- **String interning:** none — every `std::string` field is an ordinary, independently-owned string. No id-table/handle system exists anywhere in this subsystem.
- **Only one shared type exists:** confirmed — `grep -rn "class.*ActivityStatus\|struct.*ActivityStatus"` across `shared/src/` and every `sketches/*/src/` returns only this one definition. No renderer-private or scene-local duplicate was found or created.

---

## 2. Exact on-disk `EffectKnowledgePack` schema

**Header:** `shared/src/video-effects/knowledge/EffectKnowledgePack.h` (types) + `EffectKnowledgeBase.h` (`KnowledgeEntry`) + `EffectKnowledgeSerialization.h` (the actual JSON mapping, `knowledgeEntryToJson`/`knowledgeEntryFromJson`, shared by both the per-sketch `.whitelist.json`/`.blacklist.json` files and this combined pack format — see Session 1's doc for why that extraction exists).

### 2.1 Top-level pack fields

| Field | Type | Required on read? | Notes |
|---|---|---|---|
| `schemaVersion` | number | No (absent = treated as `1`) | See §5 |
| `sourceTool` | string | No | Informational only, e.g. `"shader-effect-debugger"` |
| `exportedAtUtc` | string | No | Informational only, ISO-8601-ish, `ofGetTimestampString("%Y-%m-%dT%H:%M:%SZ")` |
| `whitelist` | array of entry objects | No (absent = zero entries) | |
| `blacklist` | array of entry objects | No (absent = zero entries) | |

### 2.2 Per-entry (`KnowledgeEntry`) fields

| Field | Type | Effect or preset level (§6) | Required on read? | Absent-default |
|---|---|---|---|---|
| `schemaVersion` | int | entry-local, unrelated to pack-level version | No | `1` |
| `effect` | string | preset (identifies which effect this entry is *for*) | **Yes** — entry rejected without it | n/a |
| `list` | string (`"whitelist"`/`"blacklist"`) | preset | No | `""`; **overwritten** on pack import to match the array it was actually read from (§8) — informational only, never used to route the entry |
| `snapshot` | object, string→number | preset | **Yes** — entry rejected without it | n/a |
| `tolerance` | object, string→number | preset | No | empty map |
| `forbiddenRanges` | object, string→`{min,max}` | preset | No | empty map; a range with `min > max` is dropped (that one key only, §9) |
| `label` | string | preset | No | `""` |
| `notes` | string | preset | No | `""` |
| `sourceSketch` | string | preset | No | `""` |
| `sourceVideo` | string | preset | No | `""` |
| `timestampUtc` | string | preset | No | `""` (only auto-filled on *write*, not on read) |
| `perfObservedFps` | number, nullable | preset | No | `std::nullopt` |
| `qualityScore` | number, nullable | preset | No | `std::nullopt` |
| `sceneContext` | string | preset | No | `""` |
| `piSafe` | bool, nullable | preset (see §6 — this is the ambiguity Question 4/5 asks about) | No | `std::nullopt` (unknown — **never** coerced to `false`) |
| `compatibleSceneIds` | array of strings | preset | No | empty vector (**not** the same as "compatible everywhere" or "compatible nowhere" — see §7, Question 3) |

**Unknown top-level or per-entry fields:** silently ignored (nlohmann `ofJson`'s normal `.value()`/`.contains()` access pattern used throughout `knowledgeEntryFromJson` — there is no "reject on unknown field" strict mode anywhere in this schema).

**Ordering guarantees:** none. `whitelist`/`blacklist` arrays preserve on-disk order on read, but nothing about pack construction, effect grouping, or the export/import round trip guarantees or depends on a particular order — `resolveDominantEffectIds`/`Labels` (a *different* subsystem, §4) are the only place in this codebase where output order is a documented, tested guarantee.

**Deterministic serialization:** `exportEffectKnowledgePack()` writes via `ofSavePrettyJson`, which uses nlohmann's own key ordering (alphabetical, confirmed in the fixture below — `blacklist` before `exportedAtUtc` before `schemaVersion`...). Two exports of identical *content* produce byte-identical JSON except for `exportedAtUtc` (always the current timestamp) — not a general "reproducible build" guarantee, but stable enough that a diff between two exports only ever shows real content changes plus that one timestamp line.

**Relative vs. absolute paths:** the pack file's own *location* is resolved via `ofToDataPath(path, true)`, matching `EffectKnowledgeBase`'s existing convention — both bin/data-relative and absolute paths work. Nothing *inside* a pack (no field) is itself a filesystem path.

### 2.3 Representative fixture (real, debugger-generated — not hand-written)

Produced by `KnowledgePackSelfTest`'s actual `exportEffectKnowledgePack()` call, unedited:

```json
{
    "blacklist": [
        {
            "effect": "ascii_solarpunk",
            "forbiddenRanges": {
                "testParam": { "max": 1.0, "min": 0.8999999761581421 }
            },
            "label": "roundtrip-selftest-black",
            "list": "blacklist",
            "notes": "",
            "piSafe": false,
            "schemaVersion": 1,
            "snapshot": { "testParam": 0.9700000286102295 },
            "sourceSketch": "",
            "sourceVideo": "",
            "timestampUtc": "2026-08-07T08:07:32Z"
        }
    ],
    "exportedAtUtc": "2026-08-07T09:01:32Z",
    "schemaVersion": 1,
    "sourceTool": "shader-effect-debugger-selftest",
    "whitelist": [
        {
            "compatibleSceneIds": ["temporal-fields", "blueprint_emergence"],
            "effect": "ascii_solarpunk",
            "label": "roundtrip-selftest-white",
            "list": "whitelist",
            "notes": "generated by KnowledgePackSelfTest",
            "piSafe": true,
            "schemaVersion": 1,
            "snapshot": { "testParam": 0.41999998688697815 },
            "sourceSketch": "shader-effect-debugger-selftest",
            "sourceVideo": "none",
            "timestampUtc": "2026-08-07T08:07:32Z",
            "tolerance": { "testParam": 0.10000000149011612 }
        },
        {
            "effect": "totally_fake_effect_id_zzz_not_registered",
            "label": "",
            "list": "whitelist",
            "notes": "",
            "schemaVersion": 1,
            "snapshot": { "x": 1.0 },
            "sourceSketch": "",
            "sourceVideo": "",
            "timestampUtc": "2026-08-07T08:07:32Z"
        }
    ]
}
```

(Note the second whitelist entry: a deliberately-unknown effect id, included to demonstrate §9's rejection behavior — it is present in this *exported* fixture because export doesn't validate against a catalog, only *import* does.)

---

## 3. Absent-field defaults

| Field | Absent means | Verified by test |
|---|---|---|
| `piSafe` | **Unknown**, never `false` — `std::optional<bool>` stays `std::nullopt` | `KnowledgePackSelfTest`: round-tripped `piSafe == true`/`== false` both checked explicitly as *populated* values, distinct from absent |
| `compatibleSceneIds` | **Not yet classified**, never "no compatible scenes" — empty `std::vector`, semantically ambiguous vs. "deliberately compatible with nothing" (see §7 Q3) | `KnowledgePackSelfTest`: blacklist entry's empty `compatibleSceneIds` round-trips as empty, not fabricated |
| `presetId` | N/A — field does not exist (§6) | n/a |
| favored/blocked flag | N/A as a boolean flag — favored/blocked is *positional* (which array, `whitelist` vs `blacklist`), not a field. See §6. | `EffectKnowledgePackImportReport` counts imports per-array |
| weights | N/A — no selection-weight field exists on `KnowledgeEntry` at all (see §6 and the Session 1 doc's "genuine gap" framing — `EffectRandomizer::whitelistBiasProbability` is a *caller-supplied* request parameter, never persisted in a pack) | n/a |
| transition metadata | N/A — not serialized; `EffectActivityStatus` is in-process only (§1) | n/a |
| `schemaVersion` (pack-level) | Treated as version `1` (oldest/only version), not rejected | `KnowledgePackSelfTest`: `schemaVersionWasAbsent` checked true/false correctly per case |
| other optional effect metadata (`label`, `notes`, `sourceSketch`, `sourceVideo`, `perfObservedFps`, `qualityScore`, `sceneContext`) | Empty string / `std::nullopt`, exactly as before this session (unchanged) | pre-existing `EffectKnowledgeBase` behavior, unaffected |

**Explicit answer to "empty compatibility list vs. absent compatibility list":** they are **the same on-disk representation** — `compatibleSceneIds` is a plain `std::vector<std::string>`, not `std::optional<std::vector<std::string>>`, so JSON absence and JSON `[]` both deserialize to an empty vector. This is a real, current limitation: the schema cannot today distinguish "nobody has classified this yet" from "somebody explicitly determined this is compatible with zero scenes." Flagged, not fixed — see §7 Question 3, which is exactly the semantic-definition question Architecture needs to answer before this can be resolved correctly rather than by guessing.

---

## 4. Schema-version behavior

| Version found | Behavior | Fatal or degradable? |
|---|---|---|
| absent | Treated as `1` (`schemaVersionWasAbsent = true`, `schemaVersion = 1`), import proceeds normally | Degradable (fully proceeds) |
| `== kEffectKnowledgePackCurrentSchemaVersion` (currently `1`) | Normal import | n/a (expected case) |
| below `kEffectKnowledgePackMinSupportedSchemaVersion` (currently also `1` — no older version has ever existed) | Rejected in full: `report.ok = false`, `report.versionSupported = false`, **zero** entries imported | **Fatal** |
| above `kEffectKnowledgePackCurrentSchemaVersion` | Same as above: rejected in full | **Fatal** |
| present but not a number | Same as above: rejected in full | **Fatal** |

**Why fatal rather than degraded for an unsupported version:** a newer schema version could have silently repurposed an existing field's meaning (e.g., redefined what `compatibleSceneIds` means). There is no way to know from the reading side whether importing "the fields we happen to recognize" from an unrecognized version is actually safe — see `EffectKnowledgePack.h`'s own comment on this. No migration/adaptation logic exists (`kEffectKnowledgePackMinSupportedSchemaVersion` exists specifically so a *future* version bump has an explicit place to define one, but none is needed yet since there has only ever been one version).

**Unknown top-level/per-entry fields (distinct from unknown *version*):** always ignored, at every version, per §2.2 — this is intentionally more permissive than the version check itself, since a genuinely-unrecognized field with the *same* schema version is much safer to ignore than a whole version this reader has never seen.

Verified by `KnowledgePackSelfTest`'s `schemaVersion 999` case (§10).

---

## 5. Transition-phase model

Reused, not reinvented: `EffectActivityStatus`'s `EffectActivitySlot::phase` is `videoeffects::EvolutionPhase` (`evolution/EffectEvolutionController.h`) — `Transitioning` | `Holding`. No second transition-phase enum was created. `resolveDominantEffectIds()`/`resolveDominantEffectLabels()` roll multiple slots' phases up to one group-level `transitioning` bool (true if *any* member slot is transitioning) plus that slot's own `transitionProgress01`.

---

## 6. Effect-level vs. preset-level metadata

| Metadata | Effect-level | Preset-level | Both | Notes |
|---|---|---|---|---|
| canonical effect ID | | ✅ | | Every `KnowledgeEntry` carries its own `effect` field — there is no separate "effect record" distinct from its presets; the effect catalog itself (`VideoEffectDefinition`, `catalog/DefaultVideoEffectCatalog.cpp`) is the actual effect-level record, entirely separate from this knowledge schema |
| canonical preset ID | | | | **Does not exist.** No field, in any type, anywhere in this schema, identifies "a preset" independent of its parameter content. A `KnowledgeEntry` is identified only by `(effect, snapshot)` — see `EffectKnowledgeBase::isDuplicate()`, the only identity/equality concept this schema has |
| compatibility (`compatibleSceneIds`) | | ✅ | | Attached per-entry, i.e. per specific parameter snapshot, not per effect as a whole — a whitelist entry for `heatmap_recolor` with `gamma=1.2` could claim different scene compatibility than a different whitelist entry for the same effect with `gamma=0.7`. Whether that's the *intended* granularity or an artifact of not having an effect-level record is exactly Question 5, unresolved by this session |
| `piSafe` | | ✅ | | Same granularity note as compatibility — attached per-entry, not per-effect |
| favored | | ✅ | | Not a field — represented positionally by membership in the `whitelist` array |
| blocked | | ✅ | | Not a field — represented positionally by membership in the `blacklist` array |
| selection weight | | | | **Does not exist as persisted data.** `EffectRandomizer::RandomizeRequest::whitelistBiasProbability` is a per-call *request* parameter a caller supplies at randomization time — it is never read from or written to a `KnowledgeEntry` or a pack |
| approved parameter values/ranges | | ✅ | | `snapshot` (exact point) + `tolerance` (jitter radius) + `forbiddenRanges` (blacklist-only) — all per-entry |
| display label/name | ✅ | ✅ | | `VideoEffectDefinition::displayName` is the effect-level label (catalog-owned); `KnowledgeEntry::label` is a separate, per-entry, human-authored label for that specific preset (e.g. `"roundtrip-selftest-white"`) — two different things that happen to share the word "label" |

**Conclusion (Question 5):** the schema is **preset-level by construction**, with no effect-level record of its own — "effect-level" facts like compatibility and Pi-safety are currently *representable* only by attaching them to individual presets, which means an effect with zero authored presets has no way to carry a compatibility/Pi-safety classification at all. This is a real gap, not a bug: nothing in this session's evidence shows the current implementation "mixing" the two levels ambiguously — it simply never built an effect-level record to mix them *with*. **Recommendation, not implemented this session:** if Architecture wants effect-level (not just preset-level) compatibility/Pi-safety, that needs a new, small, additive record type (e.g. `EffectLevelKnowledge { effectId; piSafe; compatibleSceneIds; }`, one per canonical id) — deliberately not built speculatively here, since the existing preset-level fields already satisfy every acceptance criterion this session actually tested.

---

## 7. Escalation — Questions 1–6 (answered from code where possible; escalated where a contract change would be required)

### Question 1 — Is `EffectActivityStatus` sufficient for HUD Semantic Slot Model v1?

| `HUD-Semantic-Slot-Model-v1.md` §12.6 slot | Representable today? | How |
|---|---|---|
| `effects.active` (list of IDs) | ✅ | `resolveDominantEffectIds(status).effectId`, per result |
| `effects.dominant` (one ID) | ✅ | `resolveDominantEffectIds(status).front().effectId` |
| `effects.transition.progress` (0..1) | ✅ | `resolveDominantEffectIds(status).front().transitionProgress01` |
| `effects.intensity` (0..1) | ⚠️ approximate | `.front().prominence` — a caller-supplied *dominance-ranking* input, not a measured shader-intensity value; flagged as an approximation in `EffectActivityStatus.h`'s own comment, not presented as an exact match |
| `effects.health` (enum) | ❌ | **Not represented anywhere in this type.** No field maps to it. |

**This session's fix:** added `resolveDominantEffectIds()` as a new, additive function alongside the pre-existing `resolveDominantEffectLabels()` — the latter returns pre-formatted, English display strings (including a hardcoded `"(shifting)"` suffix) that must never feed a semantic-ID slot (vocabulary resolution belongs downstream, per the slot model's own `raw runtime state → semantic data → slot bindings → vocabulary → widget presentation` pipeline, §1 of that document). Before this fix, `EffectActivityStatus` had no ID-only accessor at all, which would have made a correct `effects.active`/`effects.dominant` binding impossible without either (a) parsing display text back into an ID (fragile, wrong direction) or (b) a second parallel type. Per this task's explicit instruction, no second type was created — `resolveDominantEffectIds()` is additive to the existing type.

**Escalation:** `effects.health` has no representation and this session did not invent one (no HUD slot IDs were added, per this task's own instruction). **Recommendation:** an `EffectHealth` enum (mirroring `SceneHealth`'s existing shape — `Ready`/`Loading`/`Degraded`/`Failed` or a similarly small set) is the natural fit, but its exact values and what should set them (a failed shader compile? a missing asset? Pi-throttled quality degradation?) is a real design question, not a mechanical addition — proposing it here rather than building it.

### Question 2 — Authoritative knowledge-pack path?

No canonical path is currently approved by any frozen document. `Scene-HUD-Contract-v1.md` §12's canonical asset tree names `assets/shared/video-effects/` as the eventual shared-asset location, but the *actual, currently-working* mechanism (`shared/assets/video-effects/` synced per-sketch into `bin/data/shared-video-effects/` via `scripts/sync-video-effect-assets.py`) uses different directory naming and per-sketch copies, not a single shared runtime-read location. This session's code writes/reads `shared-video-effects/knowledge/effect-knowledge-pack.json` (bin/data-relative), matching the *existing, working* synced-asset naming convention rather than the not-yet-built canonical tree. **Recommendation:** extend `scripts/sync-video-effect-assets.py` to treat the knowledge pack the same way it already treats shader assets (canonical copy under `shared/assets/video-effects/knowledge/`, synced into each consuming sketch's `bin/data/shared-video-effects/knowledge/`) — proposed, not implemented this session (real build-tooling change, outside this session's scope, flagged in Session 1's doc already).

### Question 3 — What does absent compatibility mean?

**Not resolved by this session — genuinely ambiguous in the current implementation**, and per this task's own instruction ("ground the choice in existing code and safe migration behavior," "do not choose based on convenience") this session did not pick one arbitrarily. Evidence for each candidate reading:
- *Compatible everywhere*: would match "don't restrict anything until someone actively curates it" — permissive default, but risky (an entry authored against `heatmap_recolor` on desktop could silently apply to a Pi-only scene it was never validated against).
- *Compatible nowhere*: the conservative default, but means every existing pre-Session-1 entry (all of which have empty `compatibleSceneIds`, since the field didn't exist before Session 1) would suddenly be usable by zero scenes if this reading were adopted — a behavior change for old data, not just new.
- *Unknown/unclassified*: what this session's code comments consistently describe (`EffectKnowledgeBase.h`'s own comment: *"a missing/empty compatibleSceneIds as 'not yet classified,' never as 'no scenes'"*), but "unclassified" isn't itself an actionable filtering decision — something still has to decide what an unclassified entry does when a scene asks "can I use this."

**Escalation:** this needs an explicit Architecture decision, because the three readings produce materially different runtime behavior for every entry written before this field existed. Nothing in this session touches scene-facing filtering logic that would depend on this answer (the temporal-fields integration, §8, only uses per-entry blacklist snapshot matching, which doesn't consult `compatibleSceneIds` at all), so no behavior is currently silently depending on a guessed answer.

### Question 4 — What does absent `piSafe` mean?

**Confirmed by code: `std::optional<bool>`, absent = `std::nullopt` = unknown — never coerced to `false` (unsafe) or `true` (safe).** This is enforced structurally, not just by convention: `KnowledgeEntry::piSafe` cannot hold a bare `false` for "absent," only `std::nullopt` does that, and every read path (`knowledgeEntryFromJson`) explicitly `.reset()`s before conditionally setting it, never defaults it to a boolean. Verified by `KnowledgePackSelfTest`: a round-tripped `piSafe == false` (genuinely unsafe) and a round-tripped-absent `piSafe` (unknown) are distinguishable — the self-test's whitelist entry checks `piSafe == true` explicitly rather than merely truthy, and no test anywhere treats absence as either boolean value. The type does **not** collapse "unsafe" and "unknown" into the same representation — no schema change was needed here, this session only added test coverage proving the existing three-state (`true`/`false`/absent) representation behaves correctly.

### Question 5 — Effect-level, preset-level, or both?

Answered in full in §6: **preset-level only**, no effect-level record exists. See that section's recommendation.

### Question 6 — Does schema freeze require a Decision Log update?

Yes, if/when Architecture approves this schema as frozen (per `01-architecture-governance.md`'s "effect-knowledge schema once frozen" frozen-decision listing). This session prepared the factual basis for that decision (this document + Session 1's) but does not self-approve it — no entry was added to `docs/shared-project-docs/03-decision-log.md`.

---

## 8. Debugger fix — duplicate `_main` link collision

**Root cause (confirmed, not assumed):** `sketches/shader-effect-debugger/config.make`'s `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` recursively pulls in every subdirectory, including `shared/src/video-effects/test/`, `shared/src/hud-compositor-test/`, and (as of the parallel Shared Video Playback System increment) `shared/src/video-playback/test/` — each containing a standalone `*_tests.cpp` with its own `main()`. Confirmed pre-existing and unrelated to this session's own new code by removing `shared/src/video-effects/test/` entirely from disk and reproducing the identical failure (documented in Session 1's report).

**Fix applied (narrowest correct exclusion, per this task's own instruction):**
```
PROJECT_EXCLUSIONS = ../../shared/src/video-playback/test% ../../shared/src/video-effects/test% ../../shared/src/hud-compositor-test%
```
Mirrors the exact same fix already independently applied to `sketches/blob-region-prototype/config.make` during the (separate, parallel) Shared Video Playback System increment — discovered via `git diff` on that file, not reinvented. No test program was deleted or modified to avoid the collision; each excluded directory's own `Makefile.tests` target remains fully independent and runnable (`make -C <dir> -f Makefile.tests test`).

**Result:** `make clean && make Release -j1` completes through final link — confirmed, executable exists and runs (`file` confirms Mach-O 64-bit executable; the app was actually launched and its console log captured, §9).

**A second, independent, pre-existing issue was found and worked around, not fixed:** a `-j4` (parallel) build of `temporal-fields` non-deterministically omits `shared/src/hud-compositor/widgets/HudTextMetricsCache.cpp` from compilation (no `Compiling` log line for it at all, yet the link step still expects its `.o`) — a Makefile source-discovery/parallelism race, unrelated to `shared/src/video-effects/` or this session's changes. `make Release -j1` (single-threaded) completes successfully every time. **Not fixed** — out of scope (unrelated subsystem, `hud-compositor` widgets, not video-effects), flagged as a newly-discovered risk in the final report.

---

## 9. Export/import round-trip evidence

`sketches/shader-effect-debugger/src/KnowledgePackSelfTest.{h,cpp}` — runs automatically at debugger startup (not gated behind `[k]`, so it's provable from a console log without GUI interaction, per this task's own fallback allowance for "graphical execution unavailable"). Exercises the **real** `exportEffectKnowledgePack()`/`importEffectKnowledgePack()` and **real** `EffectKnowledgeBase` persistence — nothing mocked.

**Actual console output from a real launch of the fully-linked binary** (`bin/shader-effect-debugger.app/Contents/MacOS/shader-effect-debugger`):
```
[notice ] KnowledgePackSelfTest: PASS: 42/42 checks passed
```

Covers: seed whitelist/blacklist/unknown-effect entries → export → import with real known-id validation → per-field semantic round-trip (effect id, snapshot values, tolerance, `piSafe` true *and* false, `compatibleSceneIds` populated *and* empty, `forbiddenRanges`) → unknown-effect rejection (entry never reaches the target KB under any id) → idempotent re-import (duplicate-checked, no accumulation) → absent-pack-is-not-a-failure → malformed-JSON rejection (found and fixed a real bug here, §10) → partial/malformed-entry skip (entry-level, not whole-pack) → unsupported-future-schema-version rejection (whole pack, zero entries, target KB left untouched).

**One genuine bug found and fixed by this proof, not assumed away:** `ofLoadJson()` (openFrameworks utility, `libs/openFrameworks/utils/ofJson.h`) catches nlohmann's parse exception *internally*, logs it itself, and returns whatever partially-built value its SAX parser had constructed at the point of failure — for the self-test's malformed input (`"{ this is not valid json "`), that was a non-null, `is_object() == true` empty object, which `importEffectKnowledgePack()`'s own `!root.is_object()` check could not distinguish from a genuinely well-formed empty pack. **Fix:** read raw bytes via `ofBufferFromFile()` and parse with `ofJson::parse()` directly (which throws on failure without ever assigning to `root`), rather than `ofLoadJson()`. Confirmed by the self-test going from 41/42 (this one check failing) to 42/42 after the fix.

---

## 10. Compatibility-lookup evidence

`shared/src/video-effects/test/effect_knowledge_extension_tests.cpp` (standalone, OF-free, `make -f Makefile.tests test`) — **52/52 checks passed**, including (new/extended this session):
- all six roadmap scenes present in the default matrix,
- `SharedServiceConsumer` scenes (`temporal-fields`, `blueprint_emergence`, `quadrant-crosshair`) report `sceneConsumesSharedEffects() == true`,
- `LocalForkOnly`/`NotIntegrated` scenes (`fragment-trail`, `contour-portrait`) report `false`,
- an unknown scene id (`"radar-pulse"`) returns `nullptr` from `findSceneCompatibility()` and `false` from `sceneConsumesSharedEffects()` — no crash, no silent default-true,
- every entry's `piValidated` is provably `false` (no scene has been measured on real Pi hardware — enforced as a loop-checked invariant, not a spot check).

**Preset-level compatibility override:** does not apply — confirmed in §6, no preset-level compatibility concept beyond the already-tested `compatibleSceneIds` per-`KnowledgeEntry` field, which is a different mechanism from `EffectSceneCompatibility`'s scene-level matrix (the two are deliberately not merged — one is "does this scene use the shared service at all," the other is "is this specific authored preset endorsed for that scene").

---

## 11. Dominance evidence

Same test binary, §10. New/extended this session:
- zero active instances → empty result (both `resolveDominantEffectIds` and `resolveDominantEffectLabels`),
- all slots below the prominence floor → empty result (not a crash, not a fabricated entry),
- single active instance → single result,
- multiple instances, explicit tie → deterministic winner via smallest-`slotId` tie-break (pinned down by `test_tie_breaks_by_smallest_slot_id_deterministically`),
- repeated same effect on multiple channels/slots → collapses to one result, ranked by the *max*, not sum, of member prominence,
- `resolveDominantEffectIds`/`resolveDominantEffectLabels` proven to agree on ranking order (`test_ids_and_labels_agree_on_ranking` — both call one shared internal grouping/sort implementation, so they cannot silently diverge),
- IDs never contain display text (`test_ids_never_contain_display_text` — the exact property motivating §7 Question 1's fix).

**No unstable unordered iteration:** grouping uses `std::map<std::string, EffectGroup>` (ordered by key) internally, but final ordering is always re-established by an explicit `std::sort` with a total, deterministic comparator (prominence descending, then `slotId` ascending) — `std::map`'s own iteration order is never relied upon as the final output order.

---

## 12. Canonical asset/path table

| Purpose | Path (as currently used in code) | Status |
|---|---|---|
| Shared effect *shader* asset root | `shared/assets/video-effects/` (repo-relative source of truth) | Canonical, pre-existing, unchanged by this session |
| Per-sketch synced shader assets | `<sketch>/bin/data/shared-video-effects/` (written by `scripts/sync-video-effect-assets.py`) | Canonical, pre-existing, unchanged |
| Per-sketch whitelist/blacklist knowledge | `<sketch>/bin/data/knowledge/` (via `EffectKnowledgeBase`'s default `dataDir`) | Canonical, pre-existing (Session 1's predecessor work), unchanged |
| Debugger knowledge-pack export target | `<debugger>/bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` | **Provisional** — matches the existing synced-asset naming convention but is not itself synced by any script yet |
| Scene knowledge-pack import source (this session's temporal-fields integration) | `<scene>/bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json` (same relative path, scene's own `bin/data`) | **Provisional**, same caveat — nothing currently copies the debugger's exported file into any other sketch's `bin/data/`; the import call is real and tested, but in practice finds nothing today (safe fallback, not a broken state) |
| `Scene-HUD-Contract-v1.md`'s canonical future tree | `assets/shared/video-effects/` | Approved *direction*, not yet the physical reality — different directory nesting than what's actually on disk today (`shared/assets/` vs. `assets/shared/`) |
| Self-test scratch data | `<debugger>/bin/data/knowledge_roundtrip_selftest/run_<timestamp>/` | Temporary, safe to delete, never read by production code |

**No second permanent source of truth was created.** The provisional debugger-export/scene-import path is the *same* relative path on both ends (`shared-video-effects/knowledge/effect-knowledge-pack.json`), deliberately chosen to make a future promotion to a real synced/shared location a path-string change in one place (the sync script), not a redesign.
