# Shared Effect Knowledge v1 — Schema Reference

Status: written by the **Shared Effects — Architecture-Closure Session**
(this document's own implementation pass), covering the DEC-016 closing
increment against the "Candidate for freeze after additive Architecture
resolutions" state recorded in
[`shared-effect-engineering-session-2-review`](../../../../Downloads/shared-effects-engineering-session-2-review.md)
(read from `~/Downloads/`, per this session's own authorized location — see
the final coding-agent report for that discrepancy note).

This document describes the code AS IMPLEMENTED, not as proposed. It
supersedes the schema tables in
[`shared-effect-knowledge-scoped-extension.md`](shared-effect-knowledge-scoped-extension.md)
(Session 1) and
[`shared-effect-knowledge-engineering-session-2-verification.md`](shared-effect-knowledge-engineering-session-2-verification.md)
(Session 2) wherever they disagree — those two remain useful as history of
*why* each field exists, this one is the current source of truth for
*what the field is*.

Authoritative source files (read these, not this doc, if they ever
disagree):
- `shared/src/video-effects/knowledge/EffectKnowledgeBase.h/.cpp`
- `shared/src/video-effects/knowledge/EffectLevelKnowledge.h`
- `shared/src/video-effects/knowledge/EffectPresetId.h/.cpp`
- `shared/src/video-effects/knowledge/EffectKnowledgePrecedence.h/.cpp`
- `shared/src/video-effects/knowledge/EffectKnowledgeSerialization.h/.cpp`
- `shared/src/video-effects/knowledge/EffectKnowledgePack.h/.cpp`
- `shared/src/video-effects/knowledge/EffectActivityStatus.h/.cpp`

---

## 1. Two version domains

**"Shared Effect Knowledge v1"** is the stable *project/initiative name* —
it does not change when the on-disk schema changes.

The on-disk pack format has its own independent `schemaVersion` integer,
carried in `EffectKnowledgePack::schemaVersion` /
`kEffectKnowledgePackCurrentSchemaVersion`:

| Pack `schemaVersion` | Introduced by | What changed |
|---|---|---|
| 1 | Engineering Session 1/2 | `KnowledgeEntry` with plain-vector `compatibleSceneIds`, no `presetId`, no `effectDefaults` array |
| 2 | Architecture-Closure Session (this pass) | `compatibleSceneIds` retyped to `optional<vector>` (absent vs. explicit-empty), `presetId` added, `effectDefaults` array added |

`kEffectKnowledgePackMinSupportedSchemaVersion` stays `1` — every
schemaVersion-1 pack remains fully importable (see §9, Backward
compatibility). A pack outside `[1, 2]` is rejected in full (`report.ok =
false`, nothing imported) — see
`EffectKnowledgePackImportReport`'s own doc comment for why this is
deliberately fatal, not degraded.

Per-`KnowledgeEntry` also carries its own `schemaVersion` (default `1`,
unrelated to the pack-level one, unchanged by this session) — do not
conflate the two when reading JSON by hand.

---

## 2. Preset identity (`KnowledgeEntry::presetId`)

```cpp
std::optional<std::string> presetId; // KnowledgeEntry, EffectKnowledgeBase.h
```

**Format**: `"preset.<effectId>.<slug>"`, where `<effectId>` matches the
entry's own `effect` field exactly, and `<slug>` is one or more of
`[a-z0-9_]` (validated by `isWellFormedEffectPresetId()` in
`EffectPresetId.h/.cpp`).

**Rules (DEC-016, implemented exactly):**
- stable after publication (nothing in this codebase ever rewrites an
  existing `presetId`);
- unique within one pack — enforced pack-wide (whitelist + blacklist
  together) during import, not per-list;
- associated with exactly one canonical effect ID — the `<effectId>`
  segment must equal `entry.effect`, checked at both shape level
  (`isWellFormedEffectPresetId`) and, implicitly, wherever an entry is
  constructed;
- **never** generated from vector index, file order, or a floating-point
  snapshot hash — `synthesizeMigrationPresetId(effectId, slug)` is the
  *only* sanctioned generator, and it requires the caller to supply
  `slug` explicitly (an operator-chosen name or an externally-managed
  counter), so it can never be mistaken for automatic snapshot-derived
  generation;
- may be authored manually (hand-edited JSON, or a future authoring UI)
  or generated once via `synthesizeMigrationPresetId` and then persisted;
- invalid or duplicate preset IDs are rejected **deterministically** —
  see §2.1.

**Legacy anonymous presets**: `presetId == nullopt` is a fully valid,
fully loadable entry — "written before stable identity existed, or
authored without one since." It is **not** promoted to reusable/
production-eligible status merely by existing; only
`isReusableAuthoredPreset(entry)` (well-formed + effect-matching
`presetId`) grants that status. A legacy entry remains usable for
manual/debug inspection and for the pre-existing (effect, snapshot)
content-match blacklist-avoidance path (`TFEffectPicker`'s existing use,
unaffected).

### 2.1 Import-time validation (`EffectKnowledgePack.cpp`)

Per entry with a non-null `presetId`, in order:

1. **Shape/effect-match check** (`isWellFormedEffectPresetId`) — if it
   fails, `report.skippedInvalidPresetId++`, the entry's `presetId` is
   reset to `nullopt`, and the entry **still imports** as a legacy
   anonymous preset.
2. **Pack-wide uniqueness check** (a `std::set<std::string>` scoped to
   one `importEffectKnowledgePack()` call, spanning both lists) — if the
   ID was already seen earlier in this same import, `report.
   skippedDuplicatePresetId++`, first occurrence keeps the ID, every
   later duplicate is imported as legacy-anonymous.

A malformed or duplicate ID **never** drops the whole entry — only the
identity field. Cross-import duplicate detection (against IDs already
present in the target KB from a *previous* import) is explicitly **not**
implemented — `EffectKnowledgeBase` has no presetId-indexed lookup of its
own (whitelist/blacklist are stored per-effect files, not one global
index). This is a known, documented gap (see §11), not a silent claim of
completeness.

---

## 3. Effect-level metadata (`EffectLevelKnowledge`)

```cpp
// EffectLevelKnowledge.h — pure data, no behavior, no .cpp
struct EffectLevelKnowledge {
    std::string effectId;
    std::vector<std::string> compatibleSceneIds; // empty = "no effect-level classification supplied"
    std::optional<bool> piSafe;
};
```

One record per effect ID, independent of any authored preset — an effect
can have a default classification/Pi-safety opinion even if it has zero
whitelist/blacklist entries.

**Storage** (`EffectKnowledgeBase`):
- `loadEffectLevelKnowledge(effectId) -> optional<EffectLevelKnowledge>`
  — `nullopt` if never saved (the correct "no effect-level default"
  representation; callers never receive a default-constructed empty
  record to distinguish from "genuinely absent").
- `saveEffectLevelKnowledge(const EffectLevelKnowledge&) -> bool` —
  overwrites (exactly one record per effect id, unlike whitelist/
  blacklist's many entries).
- On-disk path: `<dataDir>/<effectId>.effect-defaults.json`, one file
  per effect — same per-effect-file convention whitelist/blacklist
  already use, same atomic temp-file+rename write.

**On-disk shape** (`<effectId>.effect-defaults.json`, and identically
inside a pack's `effectDefaults[]` array):
```json
{
  "effectId": "heatmap_recolor",
  "compatibleSceneIds": ["temporal-fields", "blueprint_emergence"],
  "piSafe": true
}
```
`compatibleSceneIds` is always written as a plain array (never omitted —
`EffectLevelKnowledge` has no absent/present distinction of its own, see
§4's precedence table for what an empty array means here specifically).
`piSafe` is omitted entirely when `nullopt`.

**Pack integration**: `exportEffectKnowledgePack()` loads one
`EffectLevelKnowledge` per requested effect id (if saved) and bundles it
into `EffectKnowledgePack::effectDefaults`. `importEffectKnowledgePack()`
parses that array and calls `saveEffectLevelKnowledge()` per entry,
counted in `report.importedEffectDefaults`. Absent from any
schemaVersion-1 pack (key never existed) — imports as an empty vector,
not a parse failure.

---

## 4. Preset-level compatibility override (`KnowledgeEntry::compatibleSceneIds`)

```cpp
std::optional<std::vector<std::string>> compatibleSceneIds; // KnowledgeEntry
```

Retyped this session from a bare `std::vector<std::string>` specifically
to make **"no override authored"** and **"explicitly compatible with
zero scenes"** distinguishable — the central mechanism this closing
increment adds.

| State | Meaning |
|---|---|
| `nullopt` | No preset-level override authored — fall through to the effect-level default (§3). |
| present, non-empty | Explicit positive override: compatible with exactly these scene ids. |
| present, **empty** | Explicit override: compatible with **no** production scenes — a real, intentional statement, not "unclassified." |

**Serialization** (`EffectKnowledgeSerialization.cpp`):
- Write side tests `.has_value()`, not "non-empty" — an authored empty
  override still emits `"compatibleSceneIds": []` in JSON, never an
  omitted key (omission would be indistinguishable from `nullopt` on the
  next read).
- Read side: the field starts `reset()` (nullopt); it is only set — even
  to an empty vector — if the JSON key is **literally present**. Every
  pre-closure (schemaVersion-1) entry never had this key at all, so it
  correctly deserializes to `nullopt` ("no override, fall through"),
  never to an accidental "explicitly compatible with nothing."

---

## 5. Precedence chain (DEC-016 §3.1)

Implemented in exactly one place: `EffectKnowledgePrecedence.h/.cpp`.
Nothing else in this subsystem re-derives compatibility/piSafe/
eligibility by hand — the only two call sites are `EffectKnowledgePack.cpp`
(none currently — see §11) and `shared/src/video-effects/test/`.

```cpp
enum class KnowledgeClassification : uint8_t { Unclassified, Allowed, Disallowed };

KnowledgeClassification resolveCompatibility(
    const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault, const std::string& sceneId);
```

| preset.compatibleSceneIds | effectDefault | Result |
|---|---|---|
| has_value(), contains sceneId | — (not consulted) | **Allowed** |
| has_value(), does NOT contain sceneId (incl. explicit empty override) | — (not consulted) | **Disallowed** |
| nullopt | present, `compatibleSceneIds` non-empty, contains sceneId | **Allowed** |
| nullopt | present, `compatibleSceneIds` non-empty, does not contain sceneId | **Disallowed** |
| nullopt | absent (`nullptr`), OR present with empty `compatibleSceneIds` | **Unclassified** |

The preset override, when present, wins **outright** — the effect-level
default is never even consulted, regardless of what it would have said.
This is intentional per DEC-016 and pinned down by
`test_precedence_preset_override_wins_over_effect_default_even_when_narrower`.

### 5.1 Automatic-selection eligibility

```cpp
bool isEligibleForAutomaticProductionSelection(KnowledgeClassification classification);
// true only for Allowed. Disallowed AND Unclassified are both ineligible.
```

`Unclassified` presets remain fully inspectable/usable in manual/debug
tooling (the debugger itself never calls this function — it operates on
raw `KnowledgeEntry` lists directly) — this function only gates
*automatic* production selection, a distinction with no real call site
today (see §10).

### 5.2 Pi-safety tri-state (DEC-016 §3.2)

```cpp
std::optional<bool> resolvePiSafe(const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault);
```

| preset.piSafe | effectDefault.piSafe | Result |
|---|---|---|
| has_value() | — (not consulted) | preset's value |
| nullopt | has_value() | effect default's value |
| nullopt | nullopt / no effectDefault | `nullopt` (**Unknown**) |

"Unknown must never become true" — there is no code path that returns
`true` when both inputs are absent; the tri-state's absent case can only
ever surface as `nullopt`, never coerced.

---

## 6. `effects.health` (`EffectHealth`, `deriveEffectHealth`)

```cpp
// EffectActivityStatus.h
enum class EffectHealth : uint8_t { Ready, Degraded, Failed };

struct EffectActivityStatus {
    uint32_t schemaVersion = 1;
    EffectHealth health = EffectHealth::Ready;
    std::optional<std::string> messageId; // always nullopt when health == Ready
    std::vector<EffectActivitySlot> slots;
};

EffectHealth deriveEffectHealth(const VideoEffectLoadReport& loadReport, bool knowledgePackRejected = false);
```

Chosen over deferral: this is honestly derivable from state Shared
Effects already owns (`VideoEffectLoadReport`, populated by
`VideoEffectService::setup()`), with no dependency on scene/HUD/
RuntimeServices state.

| Condition | Result |
|---|---|
| `loadReport.missing` or `loadReport.failed` non-empty | **Failed** — something the manifest asked for could not be made usable at all. Checked first; takes priority over Degraded. |
| otherwise, `loadReport.fallback` non-empty OR `knowledgePackRejected == true` | **Degraded** — runs, but not with what was expected. |
| otherwise | **Ready** |

`knowledgePackRejected` defaults to `false` so a caller with no pack
concept (or one that hasn't imported yet) still gets the correct
Ready/Failed answer from `loadReport` alone.

**`health == Ready` with `slots` empty is a valid, common state** — "the
effect system is fine, nothing happens to be active right now." The two
fields (`health`, `slots`) are populated independently; nothing in this
type infers one from the other. Pinned down by
`test_health_ready_with_empty_active_effects_is_not_a_failure`.

`EffectActivityStatus` gains `health`/`messageId` fields but is
otherwise byte-for-byte unchanged — `EffectActivitySlot`, dominance
resolution (`resolveDominantEffectIds`/`resolveDominantEffectLabels`),
and `DominanceConfig` are untouched. Exactly one `EffectActivityStatus`
type continues to exist in this codebase; no new HUD/effect-status type
was introduced.

**Not yet wired to a real caller**: no production code currently
constructs a `VideoEffectLoadReport` and calls `deriveEffectHealth()` —
see §10.

---

## 7. `prominence` — unchanged, still dominance-only

`EffectActivitySlot::prominence` (float, caller-supplied dominance-
ranking input) is **untouched** by this session. It is not bound to
`effects.intensity`'s "how strongly a shader's own alpha/mix parameter is
currently applied" meaning — the existing header comment's caveat about
this being "a reasonable approximation, not an exact semantic match"
stands exactly as written before this session. No code change was made
here; this section exists only to record that the prohibition was
honored, not to describe new behavior.

---

## 8. Canonical path & distribution

**Sole canonical AUTHORED root** (DEC-016):
```
assets/shared/video-effects/knowledge/effect-knowledge-pack.json
```
(a *sibling* of the pre-existing `shared/assets/video-effects/` shader-
source tree — deliberately a different physical root, matching the
`assets/shared/media/` precedent from the prior Shared Video session, not
reusing `shared/assets/` which is exclusively shader/asset source.)

- **Writer**: `shader-effect-debugger`'s `ofApp::exportKnowledgePack()`
  (bound to key `k`) — the only writer. It calls the real, unmocked
  `exportEffectKnowledgePack()` against this exact path.
- **Distributor**: `scripts/sync-video-effect-assets.py`'s
  `sync_knowledge_pack()` — copies (never symlinks) the canonical file
  into every consuming sketch's
  `bin/data/shared-video-effects/knowledge/effect-knowledge-pack.json`.
  Unlike the shader-asset sync, this is **unconditional** — not filtered
  by any sketch's `effect-manifest.json`, since the pack is one bundle
  covering every effect. Sketches are discovered two ways: every sketch
  with an `effect-manifest.json` (existing shader-asset discovery), PLUS
  every sketch that calls `importEffectKnowledgePack(...)` in its own
  `src/` but has no manifest of its own (`discover_knowledge_only_
  sketches()`, which is how `temporal-fields` — a manifest-less
  consumer — gets covered without inventing a second script).
- **Reader**: any scene/sketch calling `importEffectKnowledgePack(...)`
  against its own synced copy. `TFEffectPicker::setup()` is the first
  real consumer (its bin/data-relative import path,
  `"shared-video-effects/knowledge/effect-knowledge-pack.json"`, did not
  need to change — only the sync step needed to start actually
  populating it).

**Real, unmocked end-to-end evidence captured this session**:
1. `shader-effect-debugger` launched; a real whitelist entry saved via
   key `w` (assigned a real `presetId` via `synthesizeMigrationPresetId`);
   exported via key `k` to
   `assets/shared/video-effects/knowledge/effect-knowledge-pack.json`
   (`schemaVersion: 2`, real `presetId`, real snapshot — see §12 for the
   literal file).
2. `python3 scripts/sync-video-effect-assets.py` copied that exact file,
   byte-identical (`diff` confirmed), into both
   `sketches/shader-effect-debugger/bin/data/shared-video-effects/
   knowledge/` and `sketches/temporal-fields/bin/data/shared-video-
   effects/knowledge/`.
3. `temporal-fields` launched; console log:
   `[notice] TFEffectPicker: shared effect knowledge: imported 1
   whitelist + 0 blacklist entries` — the real preset, with its real
   `presetId`, genuinely round-tripped debugger → canonical root → sync
   → Temporal.
4. Re-running the sync script a second time reported "up to date" for
   both sketches with zero writes — idempotent, as required.

A bug was found and fixed during this evidence-gathering: the pre-
existing shader-asset stale-file cleanup in `sync_sketch()` walks the
entire `bin/data/shared-video-effects/` tree removing anything not in
that sketch's shader `required` list — which included the freshly-synced
knowledge pack, since it's not a shader asset. Fixed by excluding the
knowledge pack's fixed relative path (`knowledge/effect-knowledge-
pack.json`) from that cleanup scan. Confirmed fixed by re-running the
sync twice in a row with no spurious "remove" line.

---

## 9. Backward compatibility

Every schemaVersion-1 entry/pack remains fully importable:
- `presetId` absent in JSON → `nullopt` (legacy anonymous preset).
- `compatibleSceneIds` absent in JSON → `nullopt` (no override, falls
  through to effect-level default or Unclassified) — **not** "compatible
  with nothing." This was the one behavior-preservation risk in this
  session's whole change set (a naive "empty vector on missing key"
  implementation would have silently turned every legacy entry into
  "compatible with no scenes") and is explicitly covered by both the
  serializer's own doc comment and `KnowledgePackSelfTest`'s "stayed
  absent" check.
- `effectDefaults` absent in JSON → empty vector, not a parse failure.
- Pack `schemaVersion` absent entirely → treated as version 1 (the
  oldest/only prior version), not rejected.

---

## 10. Selection-policy ownership (where weights/history/cooldown live)

Explicitly **not** in `EffectActivityStatus`, and **not** persisted into
the knowledge pack — per this session's own prohibition against putting
selection weights in the HUD/effect-status type or persisting runtime
history into authored knowledge.

**Today's actual owner**: `TFEffectPicker::Weights`
(`sketches/temporal-fields/src/TFEffectPicker.h`), a private struct owned
entirely by `TFEffectPicker` and set once via `setWeights()`:
```cpp
struct Weights {
    float cycleInterval = 8.0f;
    float rawWeight = 20.0f;
    std::map<std::string, float> effectWeights; // name -> weight
};
```
This is `temporal-fields`' own local policy — not shared, not part of
this subsystem, not touched by this session.

**Recent-history/cooldown**: no persistent history exists anywhere.
`TFEffectPicker::pickNext()` has a bounded (max 3 attempts) *within-one-
pick* retry loop that re-rolls only if the freshly-randomized parameter
snapshot content-matches a blacklist entry — this is immediate,
same-call avoidance, not a cross-pick cooldown/recency window. Nothing
writes a "recently selected" list to disk or to the knowledge pack.

**Honest finding, not glossed over**: `TFEffectPicker`'s actual
selection distribution (`pickNext()`) never consults
`compatibleSceneIds`, the whitelist, `presetId`, or
`isEligibleForAutomaticProductionSelection()` — it only consults the
**blacklist**, and only for post-hoc avoidance after already picking a
name via its own local weights. This means the precedence/eligibility
machinery built and unit-tested this session (§5) has **no real
production call site today**. This is the same category of finding as
the prior Shared Video session's `VideoSampler::requestCapture()`
dead-code discovery (later accepted by Architecture as DEC-018) — logged
here explicitly rather than silently wired into `TFEffectPicker` as an
out-of-scope behavioral change (which this session's prompt explicitly
forbade: "ExperienceRuntime/HUD renderer implementation" and any change
beyond the smallest additive increment).

---

## 11. Known gaps (not fixed this session, by design)

- **Cross-import presetId uniqueness**: only checked within one
  `importEffectKnowledgePack()` call, not against IDs already present in
  the target KB from a prior import. `EffectKnowledgeBase` has no
  presetId-indexed lookup to make this cheap; adding one is future work.
- **No production automatic-selection consumer** of
  `isEligibleForAutomaticProductionSelection()` — see §10.
- **`EffectKnowledgeBase.cpp`'s `loadList()`** still uses `ofLoadJson()`
  (which can silently return a partial/empty object on a parse error)
  rather than the `ofBufferFromFile()` + `ofJson::parse()` pattern
  `EffectKnowledgePack.cpp`'s import path already uses correctly — a
  pre-existing issue documented in Session 2's verification doc, out of
  this session's scope (whitelist/blacklist *reads*, not the pack
  import/export or effect-defaults paths this session touched).

---

## 12. Real exported pack (evidence artifact)

Captured this session, `assets/shared/video-effects/knowledge/
effect-knowledge-pack.json`, produced by the real debugger UI (not
hand-written):

```json
{
    "blacklist": [],
    "effectDefaults": [],
    "exportedAtUtc": "2026-08-08T08:58:58Z",
    "schemaVersion": 2,
    "sourceTool": "shader-effect-debugger",
    "whitelist": [
        {
            "effect": "ascii_solarpunk",
            "presetId": "preset.ascii_solarpunk.20260807_205820_1",
            "snapshot": { "...": "..." },
            "sourceSketch": "shader-effect-debugger"
        }
    ]
}
```
(abbreviated — see the real file for the full `snapshot` map.)
