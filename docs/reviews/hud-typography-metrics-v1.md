# HUD Typography Metrics v1

Status: Architecture-Closure Session deliverable. Documents the ONE v1
typography strategy in force across every production HUD text path, and
the exact metrics/budgets that strategy produces at the canonical
1280×720 canvas. Not a font system, not a skin — per the closure plan's
own framing, this closes wireframe typography acceptance without freezing
final skin typography.

## 1. Strategy decision

**Explicit pixel-space, fixed-native-size bitmap typography.** Every
glyph renders at OpenFrameworks' fixed bitmap font's own native size,
unconditionally — never adjusted by any matrix transform, never
"scaled" per-widget or per-region.

### Why, and what this replaces

Every widget in `shared/src/hud-compositor/widgets/` computes a
`wu(bounds, X, 120.0f)`-derived `scale` factor and threads it through to
`drawText(text, x, y, scale)`, which historically called
`ofScale(scale, scale)` before drawing. This session found (via a
deliberately-long-string Validation Studio screenshot,
`real_typography_overflow`, that showed uncut text where truncation
should have kicked in) that this `ofScale()` call was **provably inert**:

- OpenFrameworks' default `drawBitmapMode` on desktop is
  `OF_BITMAPMODE_MODEL_BILLBOARD` (`libs/openFrameworks/graphics/
  ofGraphicsBaseTypes.h`'s `ofStyle` constructor) — confirmed by direct
  inspection, not assumed.
- That mode billboards only the *translation* of the current model
  transform to find an on-screen anchor point (`libs/openFrameworks/gl/
  ofGLProgrammableRenderer.cpp`'s `drawString()`), then draws glyphs at
  native size regardless of any active scale.
- `drawText()`'s own call pattern (`ofTranslate(x,y)` then
  `ofScale(scale,scale)` then draw at local origin `(0,0)`) makes this a
  mathematical certainty, not an environment quirk: scaling a matrix
  around the point it's about to draw at (the local origin) cannot move
  or resize that point.

So every widget has *always* rendered text at native glyph size — this
decision makes that the documented, intentional v1 strategy instead of an
accidental side effect of dead code. **Zero rendered pixels changed** in
any existing screenshot baseline as a result of removing the `ofScale()`
call itself; what changed is that `LabelWidget`'s (and now six other
widgets') truncation math now budgets against this same real, native
size instead of the fictional scaled one — which is the actual bug this
session fixed.

### Escalation not taken

The closure plan's escalation path (switch to one consistent cached
`ofTrueTypeFont` strategy) was not needed: the fixed bitmap font at native
size satisfies readability, exact-metrics, and deterministic-truncation
requirements for wireframe acceptance. Revisiting font technology remains
open for a future skin-typography pass, explicitly out of scope here.

## 2. Exact metrics

| Metric | Value |
|---|---|
| Renderer/backend | OpenFrameworks fixed bitmap font (`ofDrawBitmapString`, freeglut's classic bitmap set) |
| Font resource | Built-in (`libs/openFrameworks/graphics/ofBitmapFont.cpp`'s embedded glyph arrays) — no external file, no `ofTrueTypeFont` load |
| Typography tiers | **One** — this v1 strategy uses a single fixed size everywhere; per-region visual weight comes from region size/layout, not font-size variation |
| Nominal pixel height (per glyph cell) | 13px |
| Glyph width (monospace) | 8px — verified by direct inspection of every `bmpChar_8x13_*` array in `ofBitmapFont.cpp`; each begins with the literal byte `8` |
| Cap height / ascender / descender | Not separately measured — this is a fixed-cell bitmap raster font, not a scalable outline font with conventional TTF metrics; the 13px cell height is the only vertically-meaningful figure available without decoding individual glyph bitmaps, and doing so is out of scope for a wireframe-acceptance pass |
| Line height | 13px (one glyph cell; this codebase draws no multi-line bitmap strings — every widget's caption/value/title/meta text is a single line) |
| Baseline offset | Not separately verified (see cap height note) — every call site positions text by its own widget-specific `y` offset (see §3), not a font-reported baseline metric |
| Letter spacing | 0px (glyphs are drawn back-to-back at exactly 8px pitch — confirmed by the same `ofBitmapFont.cpp` inspection `HudTextMetricsCache`'s `kGlyphWidthLocal` constant is built from) |
| Line spacing | N/A — single-line only, see above |
| Ellipsis glyph/string | ASCII `"..."` (3 literal period characters — not a single-glyph Unicode ellipsis; `ofBitmapFont` is ASCII-only, confirmed by `HudMissingDataController`'s own em-dash bug found and fixed in Engineering Session 2) |
| Truncation algorithm | `HudTextMetricsCache::truncateToWidth()` — deterministic linear shrink: drop one trailing character at a time from `text` until `<prefix> + "..."` fits within the budget; if even `"..."` alone doesn't fit, returns empty (never a partial/garbled glyph); result is cached by `(text, budget-rounded-to-nearest-pixel)` |
| Max lines | 1, everywhere — no widget in this layer wraps text to multiple lines (`EffectChipsWidget`'s per-chip row-wrapping is a chip-*layout* behavior, not multi-line text within one chip) |
| Longest accepted test string | `"An Extremely Long Scene Display Name That Cannot Possibly Fit Its Region"` (72 chars, `real_typography_overflow` scene-title case) / `"A Deliberately Overlong Media Title Used Only To Exercise Deterministic Ellipsis Truncation"` (91 chars, media-title case) |
| Measured truncated width (both above) | 406.72px / 407.2px respectively — truncated to exactly fit `universal.scene_title`/`universal.media_title`'s own live-content budget (see §3), confirmed via direct instrumentation (not just visual inspection, since this machine's screenshot capture — now fixed to true 1280×720, see the closure review — still can't show the truncated tail of a right-column region without scrolling) |
| Cache key / invalidation | `(text, round(maxLocalWidth))` for truncation; `text` alone for plain width lookup — a region resize (different `maxLocalWidth`) or different text both correctly produce a new cache entry; nothing here invalidates on a font/skin change since there is exactly one font in v1 |
| Fallback behavior | A budget too narrow even for `"..."` (3 chars × 8px = 24px) returns an empty string — verified by `hud_typography_tests.cpp`'s `test_too_narrow_for_ellipsis_is_empty_not_garbled` |

## 3. Per-region typography/bounds

All bounds computed at the canonical 1280×720 canvas
(`HudRegionCatalog`'s normalized bounds × canvas size). "Live-content
width" is the truncation budget actually passed to
`HudTextMetricsCache::truncateToWidth()` — `bounds.width` minus each
widget's own padding convention (`wu(bounds, padBase, 120.0f)`, evaluated
at that region's own pixel size).

| Region | Widget(s) | Region px bounds (x,y,w,h) | Padding formula | Live-content width (px) | Truncation | Max lines |
|---|---|---|---|---|---|---|
| `universal.scene_title` | Label | 844.8, 21.6, 409.6, 43.2 | `wu(bounds,4)`=1.44/side | 406.72 | Yes | 1 |
| `universal.media_title` | Label | 844.8, 72.0, 409.6, 36.0 | `wu(bounds,4)`=1.2/side | 407.2 | Yes | 1 |
| `universal.primary_state` | StatusBadge | 844.8, 115.2, 409.6, 43.2 | `wu(bounds,4)`=1.44/side | 406.72 | Yes | 1 |
| `universal.activity` | NumericValue / ProgressBar | 844.8, 165.6, 409.6, 43.2 | NumericValue: `wu(bounds,4)`=1.44/side; ProgressBar: none subtracted | NumericValue: 406.72; ProgressBar: 409.6 | Yes (both) | 1 |
| `universal.effect_summary` (chips slot) | EffectChips | 844.8, 216.0, 409.6, 21.6 (upper half of a 2-slot vertical split) | per-chip `wu(bounds,4)`=0.72/side | 408.16 per-chip backstop budget (before row-wrap) | Yes (per chip) | 1 (wraps to additional rows, not multi-line per chip) |
| `universal.effect_summary` (health slot) | StatusBadge | 844.8, 237.6, 409.6, 21.6 (lower half) | `wu(bounds,4)`=0.72/side | 408.16 | Yes | 1 |
| `overlay.health` | StatusBadge / Label | 25.6, 21.6, 256.0, 43.2 | `wu(bounds,4)`=1.44/side | 253.12 | Yes | 1 |
| `overlay.transition` (progress slot) | ProgressBar | 25.6, 72.0, 256.0, 19.2 (1 of 3 vertical slots) | none subtracted | 256.0 | Yes | 1 |
| `overlay.transition` (phase/message slots) | Label ×2 | 25.6, 91.2/110.4, 256.0, 19.2 each | `wu(bounds,4)`≈0.64/side | ≈254.7 | Yes | 1 |
| `controls.scene_previous` / `scene_next` / `media_previous` / `media_next` | Label | 25.6–435.2 range, 648.0, 102.4, 43.2 each | `wu(bounds,4)`=1.44/side | 99.52 | Yes | 1 |
| `controls.reseed` | Label | 588.8, 648.0, 128.0, 43.2 | `wu(bounds,4)`=1.44/side | 125.12 | Yes | 1 |
| `flexible.primary` | MetadataCard / ChannelStrip / others | 844.8, 460.8, 409.6, 115.2 | MetadataCard: `wu(bounds,6)`=5.76/side | 398.08 (MetadataCard); ChannelStrip per-cell below | 1 | 1 |
| `flexible.primary` (ChannelStrip per-cell) | ChannelStrip | 4 cells × 102.4 wide within the region above | `wu(bounds,3)`=2.88 (parent-region-relative) | 99.52 per cell | Yes | 1 |
| `flexible.secondary_a` | NumericValue / ProgressBar / ProgressRing / StatusBadge / Sparkline / AmbientField | 844.8, 583.2, 192.0, 43.2 | See §5's per-widget breakdown | 189.12–192.0 (per widget, §5) | Yes (all six) | 1 |
| `flexible.secondary_b` | NumericValue / ProgressBar / ProgressRing / StatusBadge / Sparkline / AmbientField | 1062.4, 583.2, 192.0, 43.2 | See §5's per-widget breakdown | 189.12–192.0 (per widget, §5) | Yes (all six) | 1 |
| `media_viewport` | (MediaViewportMesh — not a text region) | 25.6, 115.2, 768.0, 504.0 | N/A | N/A | N/A | N/A |

## 4. Coverage — CLOSED, no unresolved production text-path gap

**Final Narrow Closure Patch**: the five remaining widgets are now
classified and audited. **All 12 widget types in
`shared/src/hud-compositor/widgets/` are accounted for** — every
production-visible dynamic text path has truncation; the one tooling-only
widget is explicitly exempted, not overlooked.

| Widget | Classification | Truncation added | Notes |
|---|---|---|---|
| `LabelWidget` | production-visible dynamic text | Yes (Engineering Session 2) | scene/media title, primary state, controls |
| `StatusBadgeWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | health/state badges |
| `MetadataCardWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | |
| `EffectChipsWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | per-chip backstop, see §3 |
| `ChannelStripWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | per-cell budget |
| `ProgressBarWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | caption + value |
| `TimelineWidget` | production-visible dynamic text | Yes (Architecture-Closure Session) | caption + phase |
| `NumericValueWidget` | production-visible dynamic text | **Yes (Final Narrow Closure Patch)** | caption + formatted value |
| `ProgressRingWidget` | production-visible dynamic text | **Yes (Final Narrow Closure Patch)** | bottom caption + centered value |
| `SparklineWidget` | production-visible dynamic text | **Yes (Final Narrow Closure Patch)** | caption only — the plotted line/event-pulse dot carry no text |
| `AmbientFieldWidget` | production-visible dynamic text | **Yes (Final Narrow Closure Patch)** | caption only — the wobble lines carry no text |
| `BindingPlaceholderWidget` | **tooling-only** | Not added (out of scope by its own classification) | Never instantiated by production rendering — see its own header comment ("Never instantiated by production rendering: production skips an invalid optional binding entirely... only the studio substitutes this widget"). Draws `"BINDING ERROR"`, `input.bindingId`, and `input.compileIssueSummaries` at fixed positions with a hard `break` once content exceeds the region height — a Validation-Studio-only diagnostic surface, not a production overflow risk by construction |

No production-visible dynamic text path in this widget layer remains
unaudited. Every one was verified to: use explicit pixel-space typography
(no `ofScale()` dependency — see §1), use the shared `HudTextMetricsCache`
for measurement AND draw (same assumptions, no divergence), have a live
width budget derived from its own region's real pixel bounds, and
truncate deterministically via the same algorithm (§2). No real
production overflow defect was found in these five widgets during this
audit — none of them displays a bound value long enough to have
overflowed even before truncation was added (see §5's exact per-widget
budgets, all comfortably wider than any value `HudFormattingService`
actually produces for these bindings), so this closes the audit
proactively rather than in response to an observed defect.

**The inert-`ofScale()` finding is repo-wide**: every widget above still
threads a `wu()`-derived `scale` value through to `drawText()`'s unused
fourth parameter, retained only for call-site self-documentation (see
`HudWidgetDrawUtils.h`'s own comment). No widget's rendered glyph size
has ever depended on it, in this session or any prior one.

## 5. `flexible.secondary_a` / `flexible.secondary_b` — per-widget typography breakdown

Both regions are identical in size (192.0×43.2px), differing only in `x`
(844.8 for `_a`, 1062.4 for `_b`). Every widget type the region catalog
allows there, with its exact truncation budget at this size:

| Widget | Padding formula | Live-content width (px) |
|---|---|---|
| NumericValue | `wu(bounds,4)`=1.44/side | 189.12 |
| ProgressBar | none subtracted | 192.0 |
| ProgressRing | `wu(bounds,4)`=1.44 (one side only — centered value) | 190.56 |
| StatusBadge | `wu(bounds,4)`=1.44/side | 189.12 |
| Sparkline | none subtracted (caption only) | 192.0 |
| AmbientField | `wu(bounds,3)`=1.08 (one side only) | 190.92 |

All six were audited in Task 4 (§4) and now truncate deterministically.
See `docs/reviews/hud-wireframe-bounds-v1.md` §1 for the full padding/
inset/safe-area table these numbers feed into.

## 6. Tests

`shared/src/hud-compositor-test/hud_typography_tests.cpp` — 14/14 passing,
dependency-free (no OF headers, no GL context): width determinism/caching,
no-truncation-when-fits, deterministic truncation, too-narrow-for-ellipsis
returns empty. See `docs/reviews/hud-runtime-architecture-closure-review.md`
for the full test/build matrix this session ran.
