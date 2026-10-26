# HUD-004 extension: optional pressed artwork for NEXT SCENE (v0.1)

**Status:** Architecture-approved, backward-compatible extension. **Scope:** one optional asset, nothing more.

This note adds to the HUD-004 skin package contract. It does not replace or reopen it. The `skin.json` schema is **unchanged**: no new keys, no sidecar file, no asset discovery. A skin without this file behaves exactly as before.

## The asset

| Item | Value |
| --- | --- |
| Path (fixed, relative to the package) | `controls/scene_next/pressed.png` |
| Control | `controls.scene_next` (NEXT SCENE) |
| Region | `controls.scene_next` canonical bounds x = 153.6, y = 648, 102.4×43.2 (1280×720 canvas px, HUD-006) |
| Sprite size | **Exactly 112×52 px.** Any other size is rejected. |
| Placement | Drawn **unscaled (1:1 canvas px)**, with sprite pixel (0,0) at canvas **(149, 644)**. It covers the idle button's whole baked footprint, housing edge included, around the canonical region (HUD Art placement proposal `assets/hud/skins/scene_next/placement.json`). |
| Encoding | PNG, colour type 6 (RGBA), 8 bits per channel, non-indexed. Interlace 0 or 1. Every chunk CRC valid. These are the same structural rules as `shell.png`. |
| Colour | sRGB source imagery. It is uploaded as `GL_RGBA8` with no gamma or sRGB conversion, as with the shell. |
| Alpha | **Straight (non-premultiplied)** alpha, composited source-over (`GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`). Transparent pixels show the baked idle button in the shell underneath. |

It sits in the same HUD canvas space and final presentation transform as everything else (RT-007), so it scales with the whole composition.

**Revision (2026-10-21):** the provisional v0.1 sizes (102×43 or 205×86, stretched to the region) are replaced by HUD Art's format above. HUD Art authors **binary alpha** (0 or 255) with edges pre-blended against the shell's rail, so a sprite is valid only over the shell it was rendered with. Today that is the scaffold12 heavier-housing shell (the current `shell.png`).

## When it is drawn

- **Layer:** above `shell.png`; below every runtime label or icon and below the focus brackets.
- **When:** only during the HUD's existing accepted-activation feedback, the same 0.30 s interval and linear fade as the accent flash. No new event, command or timer exists. While the artwork shows, it replaces the accent fill for that control.
- **Never:** for a refused activation, a disabled control, a hidden or unavailable control, or an activation of a different control.
- **Focused-edge illumination:** while NEXT SCENE is focused and the skin provides this artwork, three faint inset accent hairlines are drawn inside the control's bounds. They breathe slowly, are dimmer when the control is disabled, and complement the unchanged focus brackets.

## Lifecycle

- **Load:** decoded and uploaded once during skin initialization, immediately after the required shell is fully staged, by the same image backend. It is cached as a GPU texture owned by the active skin.
- **No per-frame work:** no file access, decode or upload while rendering.
- **Release:** together with its skin, on replacement, on reload of the same skin, and on unload or shutdown. A replacement skin without the file leaves no pressed texture behind.

## Fallback

Problems with this file are **never** fatal to the skin. The skin activates and the control keeps its normal presentation, including the existing accent flash on accept.

| Condition | Result |
| --- | --- |
| File absent | Silent. Normal presentation. |
| Not a regular file, a symlink, or a symlinked parent directory | Diagnostic `SKIN_OPTIONAL_NOT_REGULAR_FILE`. Not used. |
| Unreadable | Diagnostic `SKIN_OPTIONAL_UNREADABLE`. Not used. |
| Not RGBA8, indexed, 16-bit, corrupt or truncated | The shell's PNG codes (`SKIN_PNG_*`). Not used. |
| Wrong size | Diagnostic `SKIN_OPTIONAL_WRONG_SIZE`. Not used. |
| Decoder rejects the image, or the decoded pixels aren't RGBA8 | `SKIN_LOAD_DECODE_FAILED` / `SKIN_LOAD_DECODED_FORMAT_INVALID`. Not used. |
| Texture upload fails | `SKIN_LOAD_UPLOAD_FAILED`. Not used, and nothing leaked. |
| Shell invalid | The whole skin is rejected, as before. The artwork is never staged. |

Diagnostics are logged as warnings (`HudSkin: optional asset 'controls/scene_next/pressed.png' not used (nonfatal) …`). Each skin activation also logs `optional controls/scene_next/pressed.png: loaded | absent | invalid (fallback)`.

## Authoring guidance (HUD Art)

- The artwork sits on top of the idle button baked into `shell.png`. Opaque areas hide the idle face; transparent areas show it. A pressed face that only adds light, such as an inner glow or a lit rim, works over any idle face. A face that should *replace* the idle look needs to be opaque where they differ.
- Keep the centre readable. The runtime draws the `NEXT SCENE` label on top of the artwork.
- Render the sprite together with the shell it sits on. Its pre-blended edge only matches that shell; re-render when the rail or chassis art changes.
- `focused.png` (HUD Art's optional candidate) is **not** part of this extension. It is not loaded unless separately authorized.

## Implementation references

- `shared/src/hud-compositor/HudReactiveControlArt.h`: the path, the accepted sizes and the visibility/opacity rule.
- `sketches/experience_runtime/src/HudSkinLoader.cpp`: `stageSceneNextPressed()`.
- `shared/src/hud-compositor/HudWireframeRenderer.cpp`: the overlay draw and the illumination.
- `HudCompositorBridge.cpp`: the texture hand-off.
- Tests: `test/hud_skin_loader_tests.cpp` (fixtures `skins/pressed-*`) and `ReactiveControlHarness` (`EXPERIENCE_RUNTIME_REACTIVE_HARNESS`).


## Addendum (2026-10-25): shared navigation-button faces

A skin may also supply **one shared set of three button faces**, used by all six navigation controls. They are fixed optional paths in the package. There are no `skin.json` fields, no discovery, and the manifest schema is still unchanged.

| File | When it is drawn |
| --- | --- |
| `controls/button/idle.png` | each visible control at rest |
| `controls/button/focused.png` | in place of idle while that control holds focus |
| `controls/button/active.png` | over the focused face for the existing accepted-activation interval (0.30 s, same fade) |

- **Size and placement:** each file is exactly the control region plus 4 px per side, so **80×80** for a 72×72 control. It is centred on the control and snapped to whole canvas pixels, and drawn unscaled. `regions.json` → `buttonFace` lists the six rectangles. Focus and interaction bounds stay 72×72.
- **Encoding:** RGBA8 straight alpha, the same structural rules as `shell.png`.
- **Independent and optional:** each of the three files loads on its own. An absent file is silent. A wrong-size, invalid or failed-upload file is a nonfatal diagnostic, and that state falls back (no idle face → the shell's baked housing; no focused face → idle; no active face → the accent flash).
- **Draw order:** shell, then the faces, then labels, then the focus brackets. The brackets are kept on top of the focused face.
- **Hidden controls** (for example MODE in a scene with no modes) keep the shell's baked housing and get no face.
- **Labels:** with faces present, nav-button text uses the palette's primary colour and shadow (dark ink on the marble faces).
- **Supersedes:** when a skin supplies `controls/button/active.png`, the legacy `controls/scene_next/pressed.png` (81×81) is not drawn. It is still loaded and still used when no shared active face exists.
- **Lifecycle:** all faces are decoded and uploaded once at skin load, and released with the skin. Nothing loads per frame.
