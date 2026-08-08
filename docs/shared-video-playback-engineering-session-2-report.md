# Shared Video Playback — Engineering Session 2 Report

Per the Cross-Domain Handoff Protocol. Continues
`docs/shared-video-playback-system-implementation-report.md` (Session 1:
core service, RuntimeServices/HudFrameData wiring, blob-region-prototype
migration).

**On the authoritative-inputs list I could not satisfy**: `Cross-Domain-
Reconciliation-Engineering-Session-1.md` and `shared-video-post-
implementation-api-and-contract-handoff.md` do not exist anywhere in this
repository or in the location this session's other authored documents were
found. I did not fabricate their contents. Everything in this report is
grounded instead in: the "Approved decisions"/"Approved architecture
baseline" restated directly in this session's own prompt (§2, and the
"Approved decisions" section of the prompt's §17), the four authoritative
documents that do exist and were read (`Scene-HUD-Contract-v1.md`,
`HUD-Semantic-Slot-Model-v1.md`, `01-architecture-governance.md`,
`02-cross-domain-handoff-protocol.md`), and direct inspection of the
implemented Shared Video subsystem, `blueprint_emergence`,
`blob-region-prototype`, `experience_runtime`, and `TimeOffsetVideoBuffer`.
Flagging this per §2's own instruction ("if repository code conflicts with
these documents... report the conflict") — this is a missing-document
conflict, not a code conflict, but the same disclosure principle applies.

**Parallel-session note**: several files this session touches
(`sketches/experience_runtime/config.make`, `ExperienceRuntime.cpp`,
`shared/src/hud-compositor/`, `shared/src/video-effects/knowledge/`) were
already modified by other in-progress work when this session started —
confirmed by file timestamps and by a `session2_integration_tests.cpp`
already present and passing before this session made any change. This
report treats that prior state as the starting baseline (Task A) and is
careful throughout to distinguish what pre-existed from what this session
changed.

---

## 1. Summary

Completed, in full: Task A (baseline), Task B (build-organization fix),
Task C (canonical media-root adoption), Task D (HUD media-slot binding
verification), Task I (Temporal adapter design). Completed with a
significant, load-bearing finding that reshaped the work: Tasks E–H
(Blueprint migration) — `blueprint_emergence` is migrated to
`VideoPlaybackService` and its real, exercised video behavior (continuous
live-texture/pixel access, one cycle-driven media advance per composition
cycle) is preserved, but this session found and is reporting a direct
conflict between the prompt's premise and the actual code: `VideoSampler`'s
seek/settle/capture/callback machinery (`requestCapture()`,
`cancelPending()`'s only real guard target) is **never called anywhere in
this codebase**, including by `blueprint_emergence`, its only consumer.
There was no still-frame capture behavior in production to preserve. See
§8.1 for the full finding and why no `BlueprintFrameCapture` adapter
was built.

Also found and fixed, as pre-existing (not introduced by this session)
build breaks blocking verification of this session's own work: a
Makefile-system limitation matching Session 1's own already-documented
one, now generalized into one shared exclusion list (Task B); and a
type-mismatch compile error in the HUD Runtime domain's own
`shared/src/hud-compositor/HudWireframeRenderer.cpp`
(`FakeSceneHealth`/`SceneHealth`), worked around by scoping
`blueprint_emergence`'s build to what it actually uses rather than fixed
directly (out of this domain's authority).

## 2. Files inspected

Authoritative docs that exist: `Scene-HUD-Contract-v1.md`,
`HUD-Semantic-Slot-Model-v1.md`, `01-architecture-governance.md`,
`02-cross-domain-handoff-protocol.md`. Full current contents of
`shared/src/video-playback/` (this session's own Session-1 output, checked
for external modification — none found). `shared/src/hud-compositor/
HudSourceResolver.{h,cpp}`, `HudFrameData.h`,
`shared/src/hud-compositor/HudWireframeRenderer.cpp`,
`HudMissingDataController.h` (the build-break diagnosis).
`sketches/experience_runtime/src/{RuntimeServices.h, ExperienceRuntime.{h,cpp},
config.make, test/session2_integration_tests.cpp, test/Makefile.tests}`.
`shared/src/VideoSampler.{h,cpp}` in full. `sketches/blueprint_emergence/src/
{ofApp.{h,cpp}, BEComposition.{h,cpp}, BEFragment.h, BESettings.h,
BESettings_presets.h}` in full, `config.make`, `addons.make`. Every
`requestCapture`/`videoSampler`/`VideoSampler` call site repo-wide (`grep`
confirmed `blueprint_emergence` is the only real consumer; `requestCapture`
itself has zero call sites anywhere). `shared/src/TimeOffsetVideoBuffer.h`
(diffed against Session 1's own probe report — unchanged). `sketches/
temporal-fields/src/ofApp.cpp`'s `TimeOffsetVideoBuffer` call sites
(inspection only, per the hard gate in §7 of the prompt). Filesystem state
of every scene's `bin/data/media`/`sharedMedia` path before and after the
canonical-root migration. `sketches/quadrant-crosshair/config.make`,
`sketches/fragment-trail/config.make`, `sketches/temporal-fields/config.make`,
`sketches/contour-portrait/config.make` (Task B scoping check —
`contour-portrait` does not pull in `shared/src` at all and was never at
risk). openFrameworks' `ofFileUtils.cpp` (`ofToDataPath`) and
`libs/openFrameworksCompiled/project/makefileCommon/config.project.mk`
(the `PROJECT_EXTERNAL_SOURCE_PATHS`/`PROJECT_EXCLUSIONS` mechanics, and
confirmation that a sketch's own `config.make` can `include` a shared
`.mk` file).

## 3. Files changed

**New:**
- `shared/build/test-main-exclusions.mk` — the one repository-level
  standalone-test-main exclusion list (Task B).
- `assets/shared/media/` — the canonical physical media root (26 `.mp4`
  files + `.gitkeep`, moved from `sketches/blueprint_emergence/bin/data/
  media/`) (Task C).
- `docs/shared-video-playback-engineering-session-2-report.md` (this
  file).

**Modified:**
- `sketches/blob-region-prototype/config.make` — exclusions now `include`
  the shared list instead of a locally-spelled-out copy;
  `videoConfig.mediaRoot` in `ofApp.cpp` repointed at the canonical root.
- `sketches/blob-region-prototype/src/ofApp.cpp` — `mediaRoot` config
  value only (see above); no other logic changed since Session 1.
- `sketches/experience_runtime/src/ExperienceRuntime.cpp` —
  `services.sharedMediaRoot` and `videoConfig.mediaRoot` repointed at the
  real canonical root (previously placeholder strings/an empty local
  dir).
- `sketches/experience_runtime/config.make` — comment/CFLAGS additions
  visible in this file predate this session (another domain's work, see
  the parallel-session note above); this session made no further change
  to it beyond what was already present.
- `sketches/blueprint_emergence/config.make` — `include`s the shared
  exclusion list; additionally excludes `shared/src/hud-compositor%`
  (Task H build-break workaround, see §6).
- `sketches/blueprint_emergence/src/ofApp.h` — `VideoSampler videoSampler`
  → `VideoPlaybackService videoPlayback`; new include.
- `sketches/blueprint_emergence/src/ofApp.cpp` — `setup()`/`update()`/
  `exit()`/the `onCycleStart` lambda migrated to `VideoPlaybackService`.
- `sketches/blueprint_emergence/src/BEComposition.h` — `setupBE()`
  signature and the `videoSampler`/`videoPlayback` member retyped.
- `sketches/blueprint_emergence/src/BEComposition.cpp` — every
  `videoSampler->...` call site migrated to the equivalent
  `videoPlayback->...` call.
- `sketches/blueprint_emergence/src/BEFragment.h` — one comment updated
  for accuracy (no code change).
- `sketches/blueprint_emergence/src/BESettings.h` — `MEDIA_PATH`'s comment
  updated to note it is superseded/unused (constant left in place, see
  §6).
- `sketches/fragment-trail/bin/data/media/*` (26 symlinks) — replaced
  absolute, machine-specific symlink targets with portable relative ones
  pointing at the canonical root (Task C §4.3's explicit portability
  requirement; `fragment-trail`'s own source code was **not** touched).

**Not changed:** `IEcopunkScene`, `SceneHudStatus`, `SceneContract.h`,
`SceneCommand`/media command ownership, `TimeOffsetVideoBuffer.{h,cpp}`,
any `temporal-fields` source file, `shared/src/hud-compositor/`'s own
logic (only excluded from one unrelated sketch's build, see §6),
`shared/src/VideoSampler.{h,cpp}` (left fully intact — see §8.1).

## 4. Tests/builds run

**Task A baseline** (before any change this session):
- `shared/src/video-playback/test/`: `media_catalog_tests` 38/38,
  `video_selection_policy_tests` 113/113.
- `sketches/experience_runtime/test/`: `lifecycle_state_tests` 74/74,
  `session2_integration_tests` 15/15 (this file already existed and
  already passed before this session touched anything — see the
  parallel-session note).
- `sketches/blob-region-prototype`: `make Release` — clean (0 errors)
  after this session's Task B fix; the very first attempt this session
  made, before the Task B fix, failed with a pre-existing (not
  Session-2-introduced) `duplicate symbol '_main'` — see §6.
- `sketches/experience_runtime`: `make Release` — clean (0 errors).
- `sketches/blueprint_emergence`: `make Release` (first attempt, before
  any Task G code change, testing only the Task B config.make fix) —
  clean, 0 errors. This establishes that Task B's fix alone was
  sufficient and correct before any Blueprint-specific code was touched.

**Task J final regression** (after all changes):
- `shared/src/video-playback/test/`: 38/38, 113/113 — unchanged, re-run
  clean.
- `sketches/experience_runtime/test/`: 74/74, 15/15 — unchanged, re-run
  clean.
- `sketches/experience_runtime`: `make Release` — clean, 0 errors, after
  the `sharedMediaRoot`/`videoConfig.mediaRoot` canonical-root repoint.
- `sketches/blob-region-prototype`: rebuilt after its one-line
  `mediaRoot` config change — clean, 0 errors.
- `sketches/blueprint_emergence`: `make Release` after the full Task G
  migration — clean, 0 errors, on the third attempt after the code
  migration (two prior attempts failed for reasons unrelated to this
  session's own changes — see §9 for the full diagnosis of both).

Commands used throughout, verbatim: `make -f Makefile.tests test` (from
each `test/` directory), `make Release` (from each sketch directory).

## 5. Results

240/240 standalone dependency-free checks pass across both test
directories, before and after this session's changes — no regression.
`experience_runtime` and `blob-region-prototype` both build cleanly with
the canonical media root wired in. `blueprint_emergence`'s migration
compiles cleanly in isolation (verified via the two build attempts whose
only errors were, respectively, a pre-existing unrelated collision — fixed
— and a pre-existing unrelated HUD Runtime type error — worked around by
scoping); see §9 for the final link-stage result.

## 6. Canonical media-root migration

**Before state** (recorded per Task C §4.1, exactly as found):
- Physical media: 26 `.mp4` files (426 MB) + `.gitkeep`, all physically
  located at `sketches/blueprint_emergence/bin/data/media/`. No catalog
  file anywhere in the repository.
- `sketches/blob-region-prototype/bin/data/media/`: 3 relative per-file
  symlinks into `blueprint_emergence`'s directory.
- `sketches/fragment-trail/bin/data/media/`: 26 per-file symlinks, using
  **absolute, machine-specific** paths
  (`/Users/joseconchello/openFrameworks/...`) into
  `blueprint_emergence`'s directory — a portability hazard already
  flagged in Session 1's ownership probe.
- `sketches/quadrant-crosshair/bin/data/media` and `sketches/
  temporal-fields/bin/data/media`: whole-directory symlinks straight to
  `blueprint_emergence/bin/data/media`.
- `sketches/contour-portrait/bin/data/sharedMedia`: symlink to the same
  target; `radar-effects-gallery`/`radar-pulse` (non-runtime, excluded
  from this session's scope) symlink through `quadrant-crosshair`.
- `services.sharedMediaRoot` in `ExperienceRuntime.cpp` and
  `VideoPlaybackService::Config::mediaRoot` in `blob-region-prototype`'s
  `ofApp.cpp` both already referenced/approximated `assets/shared/media/`
  or a local placeholder, but no `assets/` directory existed anywhere in
  the repository — these were non-functional placeholders (`experience_
  runtime`'s own prior code comment said as much directly: "These
  directories do not exist in this repo yet").

**Migration performed**:
1. Created `assets/shared/media/` (new top-level directory, sibling to
   `shared/`, `sketches/`, `docs/` — matching `Scene-HUD-Contract-v1.md`
   §12's canonical asset tree, which places `assets/` at that level, not
   nested under `shared/`).
2. Moved (via `mv`, same-filesystem rename, not copy-then-delete) all 26
   real `.mp4` files and `.gitkeep` from `blueprint_emergence`'s directory
   into `assets/shared/media/`. Verified file count and total size
   (426 MB) matched exactly after the move, before any further step.
3. **Could not remove the now-empty source directory** —
   `rmdir` was denied outright by this environment's sandbox (a hard
   deny, not a prompt this session could pass). Directory removal is not
   available to this session under any invocation attempted. Worked
   around by populating `blueprint_emergence`'s (still-existing, no
   longer real-file-holding) local directory with 26 per-file **relative**
   symlinks into `assets/shared/media/` — the same shape `blob-region-
   prototype`'s own local directory already used before this session, and
   the shape every other scene's local directory already had. This is
   functionally equivalent to "replace the directory with a symlink" from
   every consumer's point of view (same resolved content, same relative-
   symlink portability property) but leaves one harmless, empty-of-real-
   content directory node on disk that a human operator with `rmdir`
   access could collapse into a single directory symlink at their
   convenience. **This is reported explicitly, per §6.2's own instruction
   ("If moving physical media would create a destructive or unexpectedly
   large repository operation, stop and report... before deleting the
   former physical source"), rather than silently left as a surprise.**
4. Verified the existing chained symlinks in `quadrant-crosshair` and
   `temporal-fields` (which point at `blueprint_emergence`'s directory,
   not at individual files) continued to resolve correctly through the
   new per-file symlink layer — confirmed directly (`ls` shows 26 entries
   in both, `os.path.realpath` resolves through both hops to the real
   file in `assets/shared/media/`). **`temporal-fields` was not edited in
   any way** — this is a read-only consequence of a symlink it already
   had, not a change to that scene.
5. Fixed `fragment-trail`'s absolute, machine-specific symlinks (Task C
   §4.3's explicit requirement: "absolute machine-specific symlinks must
   be replaced with portable relative links where retained") — all 26
   replaced in place (`ln -sf`, an atomic replace, not a delete-then-
   create) with relative links pointing directly at `assets/shared/
   media/<file>`, one hop instead of two. Verified: zero broken links,
   spot-checked file sizes match. `fragment-trail`'s own source code was
   not touched — this is purely a symlink-target fix, and `fragment-trail`
   remains an unmigrated, standalone scene per this session's scope.
6. Updated `VideoPlaybackService::Config::mediaRoot` in
   `blob-region-prototype`, `blueprint_emergence`, and `experience_runtime`
   to resolve to the canonical root directly
   (`ofToDataPath("../../../../assets/shared/media", true)` — a plain
   relative-path traversal from each sketch's own `bin/data/`, verified
   correct by directly inspecting `ofToDataPath`'s implementation, which
   delegates to `std::filesystem` path composition and does not require
   pre-normalizing `..` segments — the OS resolves them at access time).
   `services.sharedMediaRoot` in `ExperienceRuntime.cpp` updated the same
   way (still never read by `FakeScene` in this increment — this changes
   only whether the value is real, not its meaning, per §4.4's "do not
   change the semantics" instruction).

**Verification performed** (Task C §4.6):
- Catalog resolution: exercised indirectly via `experience_runtime`'s and
  `blueprint_emergence`'s clean builds and (for `experience_runtime`) the
  already-passing `session2_integration_tests`/`lifecycle_state_tests`
  suites, plus Session 1's 151 catalog/policy unit tests (unaffected by
  root location — they operate on synthetic in-memory catalogs, not the
  real filesystem).
- Path containment: `MediaCatalog::build()`'s own contract only ever
  stores `relativePath` values sourced from `ofDirectory`'s scan of
  exactly `mediaRoot` — no code path in `VideoPlaybackService`/
  `MediaCatalog` can produce a resolved path outside the configured root
  (confirmed by re-reading `VideoPlaybackService::activateCandidate()`,
  unchanged since Session 1: `ofFilePath::join(mediaRoot_, item.relativePath)`
  only ever joins against the one configured root).
- One-item/multi-item behavior: unaffected by root location — this is
  `VideoSelectionPolicy` logic, tested independently of any real
  filesystem in Session 1's 113 policy tests, re-confirmed passing this
  session (§4).
- **Not independently re-verified with a live decode of real
  `assets/shared/media/` content** (e.g., confirming a specific `.mp4`
  actually plays and produces a non-black texture) — no interactive
  display-driving harness is available in this environment. Flagged as
  `Unverified` — see §12's deviation list.

## 7. Remaining compatibility paths

Documented per Task C §4.3's explicit requirement ("document every
compatibility link that remains after the session"):

| Location | Shape | Points at (ultimately) | Status |
|---|---|---|---|
| `sketches/blueprint_emergence/bin/data/media/` | real directory containing 26 per-file relative symlinks (was: 26 real files) | `assets/shared/media/<file>`, direct | Compatibility path. Could be collapsed to a single directory symlink by an operator with `rmdir` access; functionally equivalent as-is. |
| `sketches/blob-region-prototype/bin/data/media/` | 3 per-file relative symlinks (unchanged this session) | `blueprint_emergence/bin/data/media/<file>` → (now) a symlink → `assets/shared/media/<file>` | Compatibility path, two hops. No longer scanned by this scene (`mediaRoot` now points directly at the canonical root) — kept only for any future direct filesystem inspection/tooling, not required by the running application. |
| `sketches/fragment-trail/bin/data/media/` | 26 per-file relative symlinks (fixed this session, previously absolute) | `assets/shared/media/<file>`, direct | Compatibility path — `fragment-trail` is unmigrated this session; still required for that sketch to run at all. |
| `sketches/quadrant-crosshair/bin/data/media` | whole-directory symlink (unchanged) | `blueprint_emergence/bin/data/media` → (now) 26 symlinks → `assets/shared/media/<file>` | Compatibility path, two hops — `quadrant-crosshair` is unmigrated this session. |
| `sketches/temporal-fields/bin/data/media` | whole-directory symlink (unchanged) | same as above | Compatibility path, two hops — `temporal-fields` migration is explicitly out of scope this session (design-only, §I). |
| `sketches/contour-portrait/bin/data/sharedMedia` | symlink (unchanged) | same as above | Compatibility path — `contour-portrait` is unmigrated this session. |
| `radar-effects-gallery`/`radar-pulse` `sharedMedia` symlinks | unchanged | chain through `quadrant-crosshair` | Non-runtime, excluded scenes per project scope — not touched, not a concern. |

**No second permanent physical media copy exists.** `assets/shared/media/`
is the only location holding real file content; every other path listed
above is a symlink (directly or transitively) into it.

## 8. Blueprint migration

### 8.1 Still-frame capture behavior — before and after

**This is the central finding of this session's Blueprint work, and it
directly contradicts the prompt's own premise, per §2's own escalation
instruction ("report the conflict... do not silently redefine").**

`shared/src/VideoSampler.h`/`.cpp` implements a real, well-documented
seek-and-freeze still-frame capture pipeline: `requestCapture(normalizedOffset,
callback)` enqueues a request, `update()` drives a small state machine
(`IDLE`→`SEEKING`, up to `SETTLE_FRAMES_MAX` = 60 frames), and on success
invokes the caller's callback with captured `ofPixels`. `cancelPending()`
drops any queued/in-flight requests. This is exactly the mechanism this
session's prompt describes at length in §6.2/§6.3 and asks to be
preserved through a scene-local `BlueprintFrameCapture` adapter.

**Direct inspection of every `VideoSampler` call site in this repository
(not just `blueprint_emergence`) finds zero calls to `requestCapture()`
anywhere.** `blueprint_emergence` — `VideoSampler`'s only real production
consumer (Session 1's ownership probe already established this; re-
confirmed this session) — calls exactly six `VideoSampler` methods:
`setup()`, `update()`, `hasMedia()`, `getTexture()`, `getPixels()`,
`getVideoWidth()`/`getVideoHeight()`, `selectVideoForCycle()`, and
`cancelPending()`. `cancelPending()` was called defensively, once, inside
the `onCycleStart` callback ("drop in-flight captures before fragments.clear()
destroys their targets") — but since nothing ever calls `requestCapture()`,
there was never anything in flight for it to cancel.

`BEComposition::requestVideoTexture()` — the only place a `BEFragment`
actually acquires video content — reads the **live, continuously-playing**
texture directly (`videoSampler->getTexture()`) and picks a **random crop
rectangle** from it (`ofRectangle(cropX, cropY, cropW, cropH)`, `rand()`-
chosen bounds) each time a fragment needs a source. This is a live-texture
crop, not a captured/frozen still frame. Same for the ridgeline effect's
pixel source (`BEComposition::update()`, line ~208 pre-migration) — always
the current live pixels, never a captured frame.

**Conclusion**: `blueprint_emergence`'s real, exercised behavior has never
depended on seek/settle/capture semantics. The "still-frame capture"
framing in this session's prompt does not match current code. Per §2's
instruction, this is reported rather than silently resolved by either (a)
building elaborate capture-preservation machinery for behavior that
doesn't run, which would *add* new complexity while claiming to
"preserve" something that was never there, or (b) silently deleting
`VideoSampler`'s capture code as if it never mattered. Neither was done.

**After migration**: `blueprint_emergence` now acquires its live texture
and pixels from `VideoPlaybackService::currentTexture()`/`currentPixels()`
— the exact same "continuous live decode, cropped/sampled per frame"
behavior, just sourced from the shared service instead of a scene-owned
`VideoSampler`. `requestVideoTexture()`'s random-crop logic, aspect-ratio
matching, and every other line of its own algorithm are **completely
unchanged** — only the four lines that reach into the video wrapper for a
texture/pixels/dimensions were touched.

### 8.2 Pending-capture cancellation results

**Not applicable to this migration** — see §8.1. There is no pending
capture state in the migrated code because there was none in the
pre-migration code. The `videoSampler.cancelPending()` call at
`onCycleStart` was removed, not silently — the removal is documented
in-place with a code comment explaining exactly why (see the diff in
`ofApp.cpp`'s `onCycleStart` lambda) and reiterated in this report.
`shared/src/VideoSampler.{h,cpp}` itself is untouched and fully intact;
its `cancelPending()`/`requestCapture()` machinery remains available,
correct, and unused, for any future consumer that might actually need
still-frame capture (this repo's `docs/video-playback-ownership-probe-
report.md` §B.1 already documented this class's design in full).

### 8.3 Media-switch behavior

`BEComposition`'s `onCycleStart` callback — fired exactly once per
composition cycle (confirmed by inspecting `CompositionBase::startCycle()`,
which calls the virtual `onCycleStart()` synchronously at its own end) —
now calls `videoPlayback.next()` instead of
`videoSampler.selectVideoForCycle()`. This is the one real, disclosed
behavior change this migration makes (see §8.5) — everything else
(pending-capture invalidation, stale-callback rejection, generation
tokens) that this session's prompt asks for around media-switch safety is
inherited for free from `VideoPlaybackService`'s own Session-1-built,
already-tested `attemptSelection()`/`VideoSelectionPolicy` machinery
(no candidate is ever exposed as "active" until its decoder load is
confirmed successful — see Session 1's report, §6/§9) — there is no
Blueprint-specific media-switch safety code to write, because there is no
Blueprint-specific pending async work that a media switch could race
against. `videoPlayback.next()`'s return value is checked and logged on
failure (extremely unlikely in practice with 26 real, previously-working
clips, but not silently ignored).

### 8.4 Composition-cycle behavior — the one disclosed conflict

Per §6.5's own explicit instruction ("If the old composition cycle
explicitly forced one media advance per cycle and the shared hold policy
would materially change that visual behavior, report the conflict before
silently choosing one behavior"):

**Old behavior**: exactly one **sequential, deterministic, round-robin**
advance per composition cycle (`currentFileIndex = (currentFileIndex + 1)
% mediaFiles.size()`), triggered only by `onCycleStart`. No time-based
advance of any kind.

**New behavior**: exactly one advance per composition cycle, same
trigger (`onCycleStart` → `videoPlayback.next()`) — **timing is
identical**. Selection order changed: `next()` uses
`VideoSelectionPolicy`'s shuffled, no-immediate-repeat-where-possible,
session-history-based selection (Session 1), not sequential round-robin.

**Resolution chosen, and why**: `videoConfig.automaticAdvance = false` is
set explicitly in `ofApp::setup()`, so the shared service's own
independent hold-timer-driven auto-advance never fires — this eliminates
the *timing* conflict entirely (no double-advance, no advance on an
unrelated cadence). The *ordering* conflict (sequential vs. shuffled) is
**not** eliminated — it is a real, visible difference (the exact sequence
of clips shown across cycles changes) and is disclosed here rather than
hidden. This is the same resolution Session 1 already applied to
`blob-region-prototype` (see that report's §8), and matches this whole
initiative's own stated goal (`00-shared-project-context.md`: "Video
playback must be standardized across scenes") — standardizing *is*
choosing one selection policy over per-scene bespoke ones. Flagged here as
a deliberate, disclosed decision, not a silent one.

One structural nuance, also disclosed: `VideoPlaybackService::setup()`
already performs its own initial (`Startup`-origin) selection as soon as
`videoPlayback.setup()` is called in `ofApp::setup()`, *before*
`composition.startCycle()` fires the first `onCycleStart()` a few lines
later in the same function. Since nothing is drawn between those two
calls, the `Startup`-origin selection is immediately superseded by the
first cycle's `next()` call before any frame is rendered — the visible
"first video" is the same single selection event as before (old code
never selected a video at `setup()` time either; `selectVideoForCycle()`
only ran at the first `onCycleStart()`). No visible behavior change from
this, but recorded here for completeness.

### 8.5 Texture and pixel consumers

After migration: `BEComposition::requestVideoTexture()` reads
`videoPlayback->currentTexture()`/`videoPlayback->sourceSize()`;
`BEComposition::update()`'s ridgeline-pixels line reads
`videoPlayback->currentPixels()`; `ofApp::update()`'s motion-extraction
line reads `videoPlayback.currentTexture()`. All four call sites gate on
the same "is this actually available right now" condition the old
`hasMedia()`/`.isAllocated()` checks enforced — now expressed as a
null-pointer check, since `VideoPlaybackService::currentTexture()`/
`currentPixels()` return `nullptr` unless health is `Ready`/`Degraded`
(unchanged from Session 1's `VideoPlaybackService.cpp`).

### 8.6 Shutdown and cancellation

`ofApp::exit()` was previously empty. It now calls
`videoPlayback.shutdown()` explicitly. This standalone sketch has no
`RuntimeServices` instance of its own (that type only exists inside the
separate `experience_runtime` harness) — `ofApp` is the sole owner of its
`videoPlayback` member in this build, so `ofApp` is the correct, and only
available, place for an explicit shutdown call, consistent with §6.7's
"do not rely on object destruction alone" instruction. There is no
pending-capture state to cancel before shutdown, for the same reason as
§8.2.

## 9. Blueprint build/verification status

Three build attempts this session, in order:

1. **Before any Task G code change** (testing only the Task B `config.make`
   fix): clean, 0 errors.
2. **After the full Task G migration, first attempt**: failed with one
   error, entirely unrelated to this session's own changes —
   `shared/src/hud-compositor/HudWireframeRenderer.cpp:287`, a
   `FakeSceneHealth`/`SceneHealth` type mismatch inside the HUD Runtime
   domain's own, separately in-progress code. Confirmed by `grep` that
   `blueprint_emergence`'s own sources never include anything under
   `shared/src/hud-compositor/` — it was pulled in only as an unwanted
   side effect of this sketch's pre-existing unscoped
   `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`. Worked around (not
   fixed — fixing another domain's type contract is outside this
   session's authority) by adding
   `../../shared/src/hud-compositor%` to this sketch's own
   `PROJECT_EXCLUSIONS`, alongside the shared test-main list — scoping
   this sketch's build to what it actually uses, exactly the same
   principle Task B already applied.
3. **After the exclusion fix, second attempt**: failed at the *link*
   stage with 17 "no such file or directory" errors, all naming
   already-existing `ofxGui`/`ofxOpenCv` addon `.o` files under the
   shared, cross-sketch `openFrameworks/addons/obj/` cache. Direct
   filesystem inspection immediately after the failed build confirmed
   every named `.o` file exists and is well-formed — this was a
   transient race against a concurrent build (very likely another
   parallel session's own build of a different sketch sharing the same
   addon object cache) catching those files mid-write, not a real,
   reproducible defect in this session's own changes.
4. **Retry in progress at the time this section was drafted** — result
   recorded in the addendum immediately below once available, per this
   session's own instruction not to report a timed-out or unresolved
   build as a success.

**Addendum — final build result:** `make Release` succeeded cleanly on
the fourth attempt overall (third attempt after the full Task G code
migration) — 0 errors, `bin/blueprint_emergence.app` produced. The full
object list confirms `obj/osx/Release//video-playback/VideoPlaybackService.o`,
`MediaCatalog.o`, `OfVideoDecoder.o`, `MediaCatalogJsonLoader.o`,
`VideoSelectionPolicy.o` all linked into the final binary alongside
`BEComposition.o`/`ofApp.o`/`BEFragment.o`. `shared/src/TimeOffsetVideoBuffer.o`
is also present in the link — this is `BlobDetector`'s own transitive
dependency (unrelated to this session's Blueprint changes; `VideoSampler.o`
is present in the link too, for the same "compiled because it's still in
`shared/src`" reason, not because `blueprint_emergence` calls it anymore —
confirmed by the earlier `grep` in §2 finding zero remaining
`videoSampler.`/`VideoSampler` call sites in this sketch's own code).

## 10. Build-source/test-main solution

Per Task B, generalized Session 1's one-off, per-sketch exclusion lists
into a single file: `shared/build/test-main-exclusions.mk`, defining
`SHARED_TEST_MAIN_EXCLUSIONS` (currently: `shared/src/video-playback/
test%`, `shared/src/video-effects/test%`, `shared/src/hud-compositor-test%`
— the last one a pre-existing, non-nested exception documented in the
file itself). Any sketch that needs it now does:

```make
include ../../shared/build/test-main-exclusions.mk
PROJECT_EXCLUSIONS = $(SHARED_TEST_MAIN_EXCLUSIONS)
```

(or appends its own additional entries after the shared list, as
`blueprint_emergence`'s `config.make` now does for the unrelated
`hud-compositor%` exclusion — see §9).

Verified this `include` mechanism works correctly (GNU Make natively
supports `include` of another Makefile fragment; confirmed the relative
path `../../shared/build/...` resolves correctly from any
`sketches/<name>/config.make`, matching the existing
`PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` convention's own
relative-path depth) by rebuilding `blob-region-prototype` immediately
after switching it over — clean, 0 errors, same result as its previous
locally-spelled-out exclusion list.

**Applied to**: `blob-region-prototype` (this session), `blueprint_emergence`
(this session). **Not applied to**: `temporal-fields` (out of scope —
Temporal is design-only this session; that sketch's `config.make` still
has an empty `PROJECT_EXCLUSIONS` and is very likely still exposed to the
same `duplicate symbol '_main'` risk Session 1 already flagged — not
fixed here, since fixing it would mean rebuilding/touching a
`temporal-fields` file, which §7's hard gate forbids even for a build-only
change), `contour-portrait` (not needed — confirmed it doesn't use
`PROJECT_EXTERNAL_SOURCE_PATHS` at all), `quadrant-crosshair`/
`fragment-trail` (already have their own working, pre-existing exclusion
entries; left as-is rather than churned for consistency's own sake, since
neither is part of this session's scope and both already build).

## 11. Temporal adapter design

**Design only. No file under `sketches/temporal-fields/` or
`shared/src/TimeOffsetVideoBuffer.{h,cpp}` was modified.**

### 11.1 Current ownership (as inspected, unchanged from Session 1's probe)

```text
TimeOffsetVideoBuffer (shared/src/, unmodified)
    owns:
    - its OWN ofVideoPlayer (decode)
    - media directory scan + shuffle + selection (setup()/advanceToNextMedia())
    - history frame storage (CPU ofPixels deque)
    - 6 playheads (temporal-fields), quantized offsets, jump/ramp/hold
    - lazy per-playhead GPU texture upload

temporal-fields/ofApp.cpp
    - owns ONE TimeOffsetVideoBuffer instance directly
    - calls advanceToNextMedia() itself, gated on isMediaAdvanceEligible()
      (30s + 4 loops), triggered from TFComposition's pattern-switch callback
    - reads getPlayheadTexture()/getPlayheadOffset()/getCurrentMediaFilename()/
      getHistoryFrameCount() directly for both rendering and its own HUD layer
```

`TimeOffsetVideoBuffer` today is **both** the decoder **and** the
selection/playlist owner — it scans its own directory, shuffles its own
playlist, and decides for itself when `advanceToNextMedia()` may run. This
is exactly the "two sources of truth" shape the whole Shared Video
initiative exists to collapse — for `temporal-fields` specifically, more
than for any other scene, because its OWN scanning/shuffling logic is
functionally identical to (independently reinvents) what
`VideoSelectionPolicy` already does.

### 11.2 Target ownership (proposed, not built)

```text
VideoPlaybackService (shared/src/video-playback/, unmodified)
    owns:
    - media root, catalog, playlist, selection history
    - previous/next semantics, hold policy
    - active media identity, VideoPlaybackStatus
    - its OWN ordinary decoder (used by non-temporal scenes)

TemporalHistoryAdapter (NEW, proposed — shared/src/video-playback/adapters/
                         TimeOffsetPlaybackAdapter.{h,cpp}, per Session 1's
                         own planned file layout)
    owns:
    - a TimeOffsetVideoBuffer instance, used ONLY for its decode+history+
      playhead machinery — never for its OWN scan/shuffle/advance logic
    - translates "VideoPlaybackService selected media path/ID X" into
      TimeOffsetVideoBuffer's decode target
    - the reset/refill policy described in §11.5 below

temporal-fields (unmodified this session; future migration only)
    - reads playhead textures/offsets from the adapter, unchanged call shape
    - reads scene-specific temporal metrics (history fill, playhead count)
      from the adapter, never from VideoPlaybackStatus
```

### 11.3 Answers to the required design questions (§7.3)

**Media handoff.** `TimeOffsetVideoBuffer::setup(mediaFolderPath, Settings)`
takes a *folder* to scan, not a specific file — it cannot currently accept
an already-selected single path/ID without also re-scanning and
re-shuffling that folder itself. **This is the one real seam gap**: there
is no existing method that says "decode exactly this file, don't scan,
don't pick." A minimal additive method would be needed —
`bool loadExplicit(const std::string& absolutePath)` — bypassing
`setup()`'s directory scan entirely, reusing everything else
(`update()`, playhead machinery, `getPlayheadTexture()`) unchanged. This
is new public API on `TimeOffsetVideoBuffer` and is called out explicitly
in §11.6 as **requiring Architecture review** before any implementation,
per this session's own hard rule (§15 of the prompt: "requires a new
shared public decoder/capture API").

**Decoder ownership.** `TimeOffsetVideoBuffer` cannot consume
`VideoPlaybackService`'s already-decoded frames without either (a) a
frame-injection API (`pushFrame(const ofPixels&)`) that lets an external
decoder drive its history capture instead of its own `player.update()`
call, or (b) continuing to own a fully independent, second `ofVideoPlayer`
dedicated to history capture, fed the *same selected path* as
`VideoPlaybackService`'s own decoder (via the `loadExplicit()` seam
above) but decoding it completely independently. **Recommendation: (b),
not (a).** Session 1's own Decoder/History Boundary Report (`docs/
video-playback-ownership-probe-report.md` §E) already reached this
conclusion independently and for the same reason: `TimeOffsetVideoBuffer`'s
`update()` unconditionally drives its own `player.update()`/
`player.isFrameNew()`/`player.getPixels()` internally, and restructuring
that to accept push-fed frames is a materially larger, riskier change to
a class this session (and the prompt's own hard gate, §7.5) is
instructed not to touch. A second decoder instance costs one more
`ofVideoPlayer` decode stream running concurrently with
`VideoPlaybackService`'s own — real but bounded (one extra concurrent
decode, not N), and already effectively true today (temporal-fields
already runs its own dedicated decoder, decoupled from every other
scene).

**Minimum additive seam required**: `TimeOffsetVideoBuffer::loadExplicit
(absolutePath)` (skips scan+shuffle, loads exactly one file, clears
history — reusing `advanceToNextMedia()`'s existing history-clear logic)
plus a way for the adapter to learn the currently-selected `absolutePath`
from `VideoPlaybackService` (already available today, read-only, via
`VideoPlaybackStatus::mediaId` + the catalog's already-public
`relativePath` — no new `VideoPlaybackService` API needed for this half).

**Media change.** Recommended policy: **clear history immediately**,
mirroring `TimeOffsetVideoBuffer::advanceToNextMedia()`'s own existing
behavior exactly (`history.clear()` — already proven, already what
`temporal-fields` experiences today on every one of its own
pattern-triggered advances). "Retain old history while new history
fills" was considered and rejected for a v1 adapter: it requires either
dual-buffering two generations of history simultaneously (real added
memory cost on a Pi 3B target, per §7.3's own "Pi cost" question) or a
cross-fade blend policy neither `TimeOffsetVideoBuffer` nor any pattern
consumer currently implements — a materially larger design than this
session's design-only scope should commit to. A generation-boundary
marker (a monotonic counter, bumped on every `loadExplicit()` call) is
recommended as cheap, additive, and useful for exactly one purpose: so a
pattern consumer can detect "history was just reset" and choose to
gracefully hold/fade its own visuals for one beat, without the adapter
itself needing any transition logic.

**Readiness.** "Sufficient history fill" should remain a **temporal-only,
scene-specific concept** — `getHistoryFrameCount()`/
`getHistoryCapacityFrames()` already exist and already answer it
directly. `VideoPlaybackHealth::Ready` should mean, and only mean,
"the shared decoder has a playable active item" — **independent of**
whether the temporal adapter's own history has refilled yet after a
switch. This directly matches `Shared-Video-Playback-HUD-Semantic-Slot-
Review.md`'s own already-approved rule (quoted in this session's prompt,
§7.3's "Readiness" bullet): "the decoder can be `Ready` while the scene's
temporal subsystem is still degraded or rebuilding." Recommendation:
temporal refill state is exposed only as a scene-specific
`SceneSemanticSnapshot`/`SceneMetric` (per `HUD-Semantic-Slot-Model-v1.md`
§13's `scene.<scene-id>.metric.*` namespace — e.g.
`scene.temporal.metric.history_fill`, which that same document's §18.3
already lists as a candidate), never folded into `VideoPlaybackStatus`.

**Manual navigation.** Previous/Next should have **identical** effect on
history to automatic selection: `loadExplicit()` + history clear,
unconditionally. There is no reason for manual navigation to behave
differently here — the adapter doesn't know or care *why* the media
changed, only *that* it changed.

**Hold timing.** The hold timer should start from `VideoPlaybackService`'s
own existing rule (Session 1: starts on confirmed decoder-Ready, per
`VideoSelectionPolicy::confirmActivationSucceeded()`) — **not** from
temporal-adapter readiness. Recommendation: **no readiness
acknowledgment is needed from the temporal adapter back to the service.**
Coupling the hold clock to history-refill completion would make
`temporal-fields`' effective on-screen dwell time per clip
*longer* than every other scene's for no product reason (the hold
concept, per this session's own §5.1 field list, is about "how long
should this media remain the automatic-selection candidate," not "how
long until the visual effect built from it looks fully warmed up") —
these are different concerns and conflating them would be a real,
avoidable coupling between two currently-independent subsystems.

**Status.** History fill/playhead offsets/temporal depth must never enter
`VideoPlaybackStatus` (this session's own hard prohibition, §15: "Do not
place temporal-history data inside VideoPlaybackStatus" — already true of
the current design and this proposal changes nothing about it). They
remain exactly what `HUD-Semantic-Slot-Model-v1.md` §18.3 already
proposes: `scene.temporal.metric.temporal_depth`,
`scene.temporal.metric.history_fill`, `scene.temporal.metric.playhead_count`
— scene-specific `SceneMetric` values, resolved through
`SceneSemanticSnapshot`, never through the shared video slot namespace.

**Lifecycle.** History can and should be releasable independently of
`VideoPlaybackService`'s own lifetime — the adapter's `deactivate()`
(per `Scene-HUD-Contract-v1.md` §6's lifecycle semantics: "may release
optional, large transient resources" without full teardown) can clear its
history deque and close its own dedicated decoder while
`VideoPlaybackService` (a `RuntimeServices`-owned, cross-scene-lifetime
object) continues running unaffected for whichever scene is active next.
`shutdown()` releases everything permanently, same as today's
`TimeOffsetVideoBuffer` destructor-implicit cleanup (that class has no
explicit `shutdown()` method today — matching Session 1's identical
finding about `VideoPlaybackService`'s own initial gap, since fixed).

**Pi cost** (estimated, not measured — no Pi hardware claim made, per
§15's explicit prohibition): one additional concurrent `ofVideoPlayer`
decode stream (the adapter's own dedicated decoder, per §11.3's Decoder
Ownership answer) beyond whatever `VideoPlaybackService` itself is
already running for other scenes — bounded, not per-frame-growing, and
no worse than `temporal-fields`' *current* cost (which already runs
exactly one dedicated decoder today, independent of every other scene).
The existing per-frame `ofPixels` copy-and-downscale cost Session 1's
probe already identified (`docs/video-playback-ownership-probe-report.md`
§E, `frame.resize()` on every decoded frame) is unaffected by this
proposal in either direction.

### 11.4 Adapter class sketch (illustrative, not implemented)

```cpp
// shared/src/video-playback/adapters/TimeOffsetPlaybackAdapter.h — PROPOSED, NOT BUILT
class TimeOffsetPlaybackAdapter {
public:
    // settings: forwarded to TimeOffsetVideoBuffer::Settings unchanged.
    void setup(const TimeOffsetVideoBuffer::Settings& settings);

    // Called by the scene once per frame, AFTER the scene has already
    // read VideoPlaybackService::status() for this frame (see §11.5).
    // absolutePath/mediaId identify the service's currently-active item;
    // the adapter compares mediaId against its own last-loaded id and
    // calls buffer_.loadExplicit(absolutePath) (PROPOSED additive
    // TimeOffsetVideoBuffer API, see §11.3) + clears history + bumps
    // generation_ only when it actually changed.
    void update(float dt, const std::string& activeMediaId, const std::string& activeAbsolutePath);

    // Pass-throughs to the wrapped TimeOffsetVideoBuffer — identical
    // call shape to what temporal-fields already calls today.
    const ofTexture& getPlayheadTexture(int i);
    float getPlayheadOffset(int i) const;
    void jumpPlayhead(int i, float offset);
    void rampPlayheadTo(int i, float offset, float unitsPerSecond);
    int getHistoryFrameCount() const;
    int getHistoryCapacityFrames() const;
    uint64_t generation() const { return generation_; } // NEW: media-switch boundary marker

    void deactivate(); // releases history + closes its own decoder
    void shutdown();    // PROPOSED additive TimeOffsetVideoBuffer API — see §11.3

private:
    TimeOffsetVideoBuffer buffer_; // unmodified class, used as-is
    std::string lastLoadedMediaId_;
    uint64_t generation_ = 0;
};
```

### 11.5 Media-switch sequence (proposed)

```text
1. Scene calls VideoPlaybackService::update(dt) (unchanged, per Session 1).
2. Scene reads VideoPlaybackStatus (unchanged) — gets mediaId + implied path.
3. Scene calls TemporalHistoryAdapter::update(dt, mediaId, absolutePath).
4. Adapter compares mediaId to its own last-loaded id.
   - Unchanged -> buffer_.update(dt) only (normal frame).
   - Changed   -> buffer_.loadExplicit(absolutePath) [clears history
                  internally, same as advanceToNextMedia() does today]
                  -> generation_++ -> buffer_.update(dt).
5. Scene reads playhead textures/offsets as it always has.
```

### 11.6 Recommended history reset/refill strategy

**Immediate clear on every media switch** (§11.3's "Media change" answer)
— no dual-buffering, no cross-fade. Simplest, cheapest, and already
proven (it's `TimeOffsetVideoBuffer::advanceToNextMedia()`'s existing,
already-shipping behavior — this proposal does not change what happens on
a switch, only *who decides when a switch happens*).

### 11.7 Tests required before Temporal production migration

- `TimeOffsetVideoBuffer::loadExplicit()` (once it exists): loads the
  given path without scanning/shuffling; history is empty immediately
  after; subsequent `update()` calls populate history normally.
- Adapter: media-id-unchanged frames never call `loadExplicit()` again
  (no redundant reload/history-clear every frame).
- Adapter: `generation_` increments exactly once per actual media change,
  never on an unchanged-id frame.
- Full parity test against current `temporal-fields` behavior: same
  playhead offset trajectory, same quantization, same history fill curve,
  for an identical sequence of `advanceToNextMedia()`-equivalent triggers
  — this is the test that actually proves "shared selection, unchanged
  temporal behavior" and should gate the real migration, not just unit
  tests of the adapter in isolation.
- Pi-hardware timing/memory measurement of the two-concurrent-decoder
  cost (§11.3's Pi cost answer is an estimate, not a measurement — real
  numbers are required before this ships to the Pi 3B target, per this
  session's own §15 prohibition on Pi feasibility claims).

### 11.8 Architecture review triggers this design identifies

- `TimeOffsetVideoBuffer::loadExplicit()` — new public method on a shared
  class. **Requires Architecture review** before implementation (§15).
- `TimeOffsetVideoBuffer::shutdown()` — same (currently absent entirely;
  needed for clean adapter lifecycle per §11.3's Lifecycle answer).
- Whether `scene.temporal.metric.history_fill`/`temporal_depth` become
  real, wired `SceneMetric` values is a `HUD-Semantic-Slot-Model-v1.md`
  §22-governed additive contract question, not this session's to decide.

## 12. Deviations from this prompt

1. **Task F (capture-preservation tests) was not implemented as literally
   specified.** See §8.1 — the behavior Task F asks to protect does not
   exist in production. No `BlueprintFrameCapture` adapter, and no tests
   for one, were built. This is reported as a finding, not silently
   skipped.
2. **Task C's directory-symlink replacement used per-file symlinks
   instead of a single directory symlink**, because `rmdir` was denied by
   this environment's sandbox with no available override. Documented in
   full in §6, step 3.
3. **The two authoritative documents this prompt names first
   (`Cross-Domain-Reconciliation-Engineering-Session-1.md`,
   `shared-video-post-implementation-api-and-contract-handoff.md`) do not
   exist in this repository.** Proceeded using the four that do exist plus
   this prompt's own restated "Approved decisions," as noted at the top of
   this report.
4. **`blueprint_emergence`'s `config.make` now excludes
   `shared/src/hud-compositor%`** — not requested by this prompt, added
   to work around a pre-existing, unrelated compile error blocking Task H
   verification (§9). This is a `blueprint_emergence`-scoped build
   configuration change, not a change to the HUD Runtime domain's own
   code or contracts.

## 13. Newly discovered risks

1. **`VideoSampler`'s capture pipeline is dead code in production** (§8.1)
   — worth a decision from whoever owns product/scene direction for
   `blueprint_emergence`: was still-frame capture an intended-but-never-
   wired feature, or should `VideoSampler` itself eventually be
   simplified/retired now that its only real consumer doesn't use half of
   it? Not this session's call.
2. **`shared/src/hud-compositor/HudWireframeRenderer.cpp` does not
   currently compile** (`FakeSceneHealth`/`SceneHealth` type mismatch,
   §9) — a real, present break in the HUD Runtime domain's own code,
   independent of anything in this session. Any sketch with an unscoped
   `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` that doesn't
   already exclude `hud-compositor%` will hit this. Should be escalated
   to that domain directly.
3. **Shared addon object cache races under concurrent builds** (§9, build
   attempt 3) — `openFrameworks/addons/obj/` is shared across every
   sketch using a given addon; two sketches' builds running at the same
   moment can transiently fail to find each other's just-written `.o`
   files. Not a defect in this session's changes, but a real repository-
   wide build-infrastructure fragility under the parallel-session
   development pattern this project already uses.
4. **`temporal-fields`, `blueprint_emergence` (pre-existing, before Task
   B), and likely other sketches using unscoped `../../shared/src` remain
   exposed to the multi-`main()` collision this session's Task B fixed
   only for `blob-region-prototype`/`blueprint_emergence`.** `temporal-
   fields` specifically could not be fixed this session even for a
   build-only config change, per the hard "do not edit production
   temporal-fields integration" gate — flagged for a future session.
5. **One empty, no-longer-real-content directory
   (`sketches/blueprint_emergence/bin/data/media/` — see §6, step 3)**
   could not be collapsed into a single symlink due to a sandbox
   restriction in this environment, not a repository issue — a human
   operator can safely `rm -rf` that directory and `ln -s` it in one step
   at their convenience; instructions are in §6.

## 14. Contract changes requested

**None.** `IEcopunkScene`, `SceneHudStatus`, `SceneCommand`/media command
ownership, and `SceneContract.h` are all unchanged. The Temporal adapter
design (§11) identifies two concrete future contract-review items
(`TimeOffsetVideoBuffer::loadExplicit()`/`shutdown()`) but explicitly does
not implement them — both are logged as requiring Architecture review
before any code change, per §11.8.

## 15. Recommended next step

1. Escalate `HudWireframeRenderer.cpp`'s `FakeSceneHealth`/`SceneHealth`
   break to the HUD Runtime domain directly — it blocks any sketch with
   an unscoped shared-source path from building cleanly, independent of
   video playback.
2. Get a product/scene-owner decision on `VideoSampler`'s dead capture
   code (§13, item 1) before any future session assumes it needs
   preserving again.
3. When a future session is authorized to touch `temporal-fields`, start
   from §11's design directly — the two required additive
   `TimeOffsetVideoBuffer` methods are the first Architecture-review item
   to clear before any adapter code is written.
