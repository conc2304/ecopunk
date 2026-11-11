# crt-phosphor (experimental, standalone)
Green-phosphor CRT video treatment. Not a production scene; not registered with SceneManager.
Build/run: `cd sketches/crt-phosphor && make Release -j4 && make RunRelease`
Media: `bin/data/media/` if it has clips, else `assets/shared/media/` (SharedMediaRoot.h). Falls back to a procedural pattern.
Shaders are embedded in `src/CrtShaders.h` (GLSL 1.20; Pi/GLES2 untested). `CRT_SHADER_DIR=<dir>` + R reloads `<dir>/{base,bright,blur,composite}.frag`.
Keys: B bypass, L bloom, P persistence, 1/2/3 preset, D debug, R reload, S screenshot, N/M video, V video/pattern, F cover/fit, Q quality, T per-pass timing.
Env: CRT_VIDEO=<substr>, CRT_SOURCE=pattern, CRT_AUTOSHOT_DIR=<dir> (captures reference/clean/damaged, prints stats, exits), CRT_BYPASS/CRT_NO_PERSIST/CRT_NO_BLOOM.

## CRT Display Lab (orthogonal axes; none of these are composition modes)
Pipeline: source -> composition MODE (signal frame) -> [behavior sampling] -> STRUCTURE program (output-pixel emitter geometry)
-> COLOR MAPPER (palette axis, dither anchored to logical cells) -> shared bloom -> persistence/analog composite.
- Axes (stable IDs): structure `original_phosphor|rgb_triad|aperture_grille|round_matrix|ascii_matrix|vector_beam`;
  color mapping `classic_mono|theme_primary_secondary|source_rgb|ega16|sunset|biolum|thermal|four_tone|dual_phosphor`;
  behavior `none|adaptive_mosaic|floating_phosphors|interlaced_color_fields`; dither off/low(2x2)/high(4x4); seed.
- Keys: x/X structure, z/Z color mapping, a/A behavior, g dither, y seed, -/= pitch, \ dense, e calm/energized floating,
  0 reset to reference, w hide dev panel, o save recipe (bin/data/recipes/*.json, sketch-local), i load next recipe.
  Existing: [ ] composition mode, F1-F4 direct, 1/2/3 look, C theme palette, B/L/P/D/R/S/N/M/V/F/Q/T/H/J/K/,/. unchanged.
- Compatibility (visible in the panel; changes are rejected or behavior resets to none, never an alternate look):
  adaptive_mosaic + floating_phosphors: round_matrix only. interlaced_color_fields: original, rgb_triad, aperture_grille, round_matrix.
  ascii_matrix / vector_beam: behavior none only.
- Env: CRT_STRUCTURE / CRT_CMAP / CRT_BEHAVIOR (IDs), CRT_FIXED_DT, CRT_FREEZE=<pos 0..1>,
  CRT_GALLERY=1 + CRT_AUTOSHOT_DIR=<dir> (contact sheets + per-structure GPU timings, exits).
