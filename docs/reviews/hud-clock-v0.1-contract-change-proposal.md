# HUD Clock v0.1 — Contract Change Proposal

Status: **PROPOSED — awaiting Architecture review. No code implemented.**
Date: 2026-10-15
Raised by: HUD Runtime coding session (Clock v0.1 task)
Process: `docs/shared-project-docs/01-architecture-governance.md` § Contract-change process

## 1. Motivation

Clock v0.1 asks for a timezone-aware local date/time display in the
bottom-right HUD region (`flexible.secondary_b`), sampled by the runtime from
the OS wall clock, DST-aware through OS timezone facilities, cached, and
read-only.

## 2. Current limitation (why coding stopped)

The display side fits. The data path does not exist:

- `HudFrameData` (`shared/src/hud-runtime/HudFrameData.h`) carries no
  wall-clock or timezone information. Its members are `SceneFrame`,
  `SceneHudStatus`, `SceneManagerStatus`, `SceneCapabilities`,
  `RuntimeTelemetry` (fps, frame time, CPU temperature, resident memory,
  throttled), `video` and `effects`.
- The header states: "Any change to this shape is a shared-contract change."
- Governance § Architecture review triggers: "adds or changes a shared public
  type" and "changes scene/HUD status shape". § Frozen shared decisions:
  "semantic slot IDs". A clock needs a new `HudFrameData` field **and** new
  semantic source IDs, which hits all three.
- No approved slot, field or document defines wall-clock time. Repository
  search for clock, wall-time, timezone and local-time slots in
  `docs/shared-project-docs/`, `SceneContract.h` and `HudSourceResolver.cpp`
  found nothing.
- Governance: "Coding work begins only after the artifact is updated."

### What does fit without any frozen change (verified)

| Concern | Finding |
|---|---|
| Geometry | `flexible.secondary_b` = 1062.4, 583.2, 192.0 × 43.2 px (HUD-006 crosswalk). Unchanged. |
| Widget | `NumericValue` is already accepted there. The live text budget is 189.12 px = 23 glyphs at 8 px (`hud-wireframe-bounds-v1.md` §1a, `hud-typography-metrics-v1.md`). It draws two 13 px lines (caption + value) inside 43.2 px. |
| Value typing | `NumericValue.value` accepts `DurationSeconds`. Seconds since local midnight (0–86399) is exact in the resolver's `float`, whereas Unix epoch seconds would **not** be (about 128 s resolution at 1.7e9). |
| Dynamic date/zone line | `NumericValue` already contracts an optional `label` role accepting `Text` (`HudWidgetContract.cpp`), but the widget does not draw it today. Drawing an already-contracted role is HUD-internal implementation. |
| Production visibility | The MinimalSlice allowlist is a permitted HUD-006 subset (Living HUD v0.1 precedent). |
| Skin | No manifest or asset change. |

## 3. Proposed change (smallest)

### 3a. Shared type (new, OF-free) — `shared/src/hud-runtime/WallClockStatus.h`

```cpp
struct WallClockStatus {
	int64_t unixSeconds = 0;          // UTC instant (std::chrono::system_clock), whole seconds
	int32_t utcOffsetSeconds = 0;     // OS-resolved offset for THIS instant, DST included
	std::string zoneAbbreviation;     // OS %Z, e.g. "CEST"; empty = OS gave none
	std::optional<bool> daylightSaving; // tm_isdst: >0 true, 0 false, <0 nullopt (unknown)
};
```

Raw facts only, with no formatted strings in the contract. Formatting stays HUD
presentation.

### 3b. `HudFrameData` — one additive field

```cpp
// Absent (nullopt) when the runtime could not obtain a wall-clock sample or
// its local-time conversion failed this frame. Never fabricated.
std::optional<WallClockStatus> wallClock;
```

Same additive-optional pattern as `video` (Shared Video) and `effects`
(DEC-015). It is assembled by ExperienceRuntime only, never by a scene, so it
carries no scene-specific clock data.

### 3c. Semantic source IDs (two)

| Source ID | Value type | Meaning |
|---|---|---|
| `runtime.clock.local_seconds` | `DurationSeconds` | seconds since local midnight, `(unixSeconds + utcOffsetSeconds) mod 86400` |
| `runtime.clock.local_label` | `Text` | HUD-formatted `"THU 15 OCT CEST"`, or a numeric `"UTC+05:30"` when the abbreviation is empty |

Both resolve **missing** when `wallClock` is nullopt (Hide policy, so the region
stays recessed).

### 3d. Ownership and cadence

- **Sampling:** ExperienceRuntime reads `system_clock::now()` each frame
  (a clock read only), and calls `localtime_r` **only when the whole second
  changes**, caching the `WallClockStatus`. So the OS conversion runs at most
  1 Hz.
- **HUD formatting:** `HudFormattingService`/resolver caches the two
  strings keyed on (local minute, offset, abbreviation), rebuilding about once a
  minute plus on any offset/zone change. Display is HH:MM, so there's no
  per-frame text churn.
- **No new runtime service** (stays inside ExperienceRuntime's existing
  frame assembly). If Architecture prefers RuntimeServices ownership, that is
  a "new runtime service" trigger and should be decided here.

## 4. HUD-internal work once approved (domain-owned, no further contract change)

1. `HudFormatKind::ClockTime` → `"14:32"` from `DurationSeconds`.
2. `NumericValueWidget` draws its contracted `label` role (when present and
   rendered) in place of the static caption line; existing bindings, which
   have no `label` source, render byte-identically.
3. Universal-profile binding `universal.clock.value` → `flexible.secondary_b`:
   `NumericValue`, `value = runtime.clock.local_seconds`,
   `label = runtime.clock.local_label`, `HudMissingPolicy::Hide`.
4. Production allowlist part 3: `flexible.secondary_b`.
5. Read-only: no `enabled` role, so `HudFocusNavigator` never sees it as a
   control.

Rendered (worst case 22 of 23 glyphs):

```
THU 15 OCT CEST        <- label line (muted)
14:32                  <- value line (primary)
```

## 5. Alternatives considered

| Alternative | Why not |
|---|---|
| Add fields to `RuntimeTelemetry` (`SceneContract.h` §13) | Still a shared-contract change. `SceneContract.h` is also scene-facing and telemetry semantics don't fit wall time. |
| HUD samples the OS clock itself inside `HudWireframeRenderer` | No contract edit, but it breaks the "HudCompositor only receives `HudFrameData` by const&" boundary, makes Validation Studio/fake scenarios non-deterministic, and is not runtime-owned, as the task requires. |
| Single `Text` source with full `"2026-10-15 14:32 CEST"` in a `Label` | `Label` is not accepted in `flexible.secondary_b`, so that would be a frozen HUD-006 crosswalk change. `NumericValue.value` rejects `Text`, so that would be a widget-contract change. |
| Unix seconds as `Scalar` | Float precision is about 128 s, so it's incorrect. |

## 6. Migration cost and affected domains

- **ExperienceRuntime:** about 40 lines of sampling + assembly; one unit test file.
- **HUD Runtime:** resolver entries, one format kind, one widget draw branch,
  one binding, one allowlist entry; tests.
- **HUD Art:** the production region count moves from 15 to 16. `HudArtReferenceExporter`
  references (`docs/hud-art/blueprint-v1-reference/`) need regeneration, so a
  handoff is required.
- **Validation Studio / fake scenarios:** fake frames leave `wallClock`
  nullopt or supply a fixed instant, keeping them deterministic.
- **Scenes:** none. **Skin:** none. **Blueprint geometry:** none.

## 7. Deterministic tests planned

**Pure HUD formatter** (fed a `WallClockStatus`, no OS calls):
- date rollover at local 23:59:59 → 00:00:00;
- midnight exactly, where `local_seconds` is 0;
- year and leap-day rollover (2028-02-28/29 → 03-01);
- a negative offset crossing UTC midnight;
- an empty abbreviation giving the `UTC±HH:MM` fallback;
- a half-hour offset (+05:30);
- `wallClock` nullopt resolving both sources missing (recessed).

**Runtime sampler** (POSIX `TZ` + `tzset()`, fixed instants):
- the America/New_York spring-forward 2026-03-08 and fall-back 2026-11-01
  transitions, checking offset, abbreviation and `isdst` on both sides;
- Europe/Berlin and Australia/Sydney (southern-hemisphere DST);
- an invalid `TZ` string, which glibc and macOS both treat as UTC, so the test
  asserts offset 0 with a reported abbreviation;
- a `localtime_r` failure giving nullopt.

## 8. Raspberry Pi limitations (known before implementation)

- **No RTC on Pi 3B.** Until NTP syncs, wall time is wrong: 1970, or the
  last-shutdown time restored by `fake-hwclock`. That is offline kiosk
  risk. v0.1 would display it as-is. A future `synchronized` flag would need
  a source (e.g. `timedatectl`/`adjtimex` status) and is out of scope here.
- **Timezone** comes from `/etc/localtime`/`TZ`. A wrong image setting shows a
  wrong but correctly DST-adjusted time. A stale `tzdata` package mis-handles
  future rule changes.
- **Locale:** month and day names must be fixed ASCII tables, not locale
  `strftime`, because the bitmap font is ASCII-only.
- **Cost:** one `system_clock` read per frame plus at most 1 Hz `localtime_r` and
  about 1/min string builds. That's negligible on the Pi 3B.

## 9. Decision requested

Approve or amend §3a–§3c (type, field, two source IDs) and §3d ownership. On
approval, HUD Runtime implements §4 and §7 with no further contract changes.
