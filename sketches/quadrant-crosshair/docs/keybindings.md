# Keybindings — quadrant-crosshair

## Normal mode

| Key | Action |
|-----|--------|
| `1` – `5` | Switch crosshair preset |
| `Tab` | Cycle to next crosshair preset |
| `n` / `N` | Advance to next video file |
| `f` / `F` | Toggle fullscreen |
| `h` / `H` | Toggle HUD overlay |
| `[` | Decrease motion overlay opacity (–10) |
| `]` | Increase motion overlay opacity (+10) |
| `o` / `O` | Toggle motion overlay depth: behind quadrants ↔ in front of quadrants |
| `m` / `M` | Cycle motion extraction output mode (luma glow → chroma preserve → signed field) |
| `e` / `E` | Trigger expansion sequence (random quadrant) |
| `F1` | Trigger expansion on quadrant 0 (top-left) |
| `F2` | Trigger expansion on quadrant 1 (top-right) |
| `F3` | Trigger expansion on quadrant 2 (bottom-left) |
| `F4` | Trigger expansion on quadrant 3 (bottom-right) |
| `d` / `D` | Enter / exit debug mode |
| `Esc` | Quit |

---

## Debug mode (`d` to enter, `d` to exit)

Full-screen shader preview with live parameter tweaking and manual trigger firing.

### Shader navigation

| Key | Action |
|-----|--------|
| `←` | Previous shader |
| `→` | Next shader |

### Parameter adjustment

| Key | Action |
|-----|--------|
| `↑` / `↓` | Select previous / next parameter |
| `=` | Increase selected parameter (fine step) |
| `-` | Decrease selected parameter (fine step) |
| `+` (Shift `=`) | Increase selected parameter (coarse ×10) |
| `_` (Shift `-`) | Decrease selected parameter (coarse ×10) |

### Trigger simulation

Toggle keys hold the trigger active until pressed again. Fire-once keys pulse for 0.5 s.

| Key | Trigger | Type |
|-----|---------|------|
| `E` | EDGE_PROXIMITY | toggle |
| `V` | VELOCITY_HIGH | toggle |
| `L` | VELOCITY_LOW | fire-once |
| `C` | QUADRANT_CENTER | toggle |
| `K` | CORNER_NEAR | toggle |
| `W` | DWELL | fire-once |
